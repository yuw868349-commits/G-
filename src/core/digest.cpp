#include "core/digest.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace swiftagent {

static std::string join(const HardKeys& keys) {
    std::string out;
    for (const auto& [k, v] : keys) {
        out += k;
        out += '=';
        out += v;
        out += '\n';
    }
    return out;
}

HardKeys Normalizer::hard_keys(const std::string& content) {
    HardKeys keys;
    if (content.empty()) {
        return keys;
    }
    try {
        auto json = nlohmann::json::parse(content);
        if (json.is_object()) {
            for (auto it = json.begin(); it != json.end(); ++it) {
                if (it.value().is_string()) {
                    keys[it.key()] = it.value().get<std::string>();
                } else if (it.value().is_number_integer()) {
                    keys[it.key()] = std::to_string(it.value().get<long long>());
                } else if (it.value().is_number_float()) {
                    keys[it.key()] = std::to_string(it.value().get<double>());
                } else if (it.value().is_boolean()) {
                    keys[it.key()] = it.value().get<bool>() ? "true" : "false";
                }
            }
        }
    } catch (const nlohmann::json::exception&) {
        // Undocumented layout: no hard keys extracted; soft tier covers it.
    }
    return keys;
}

bool Normalizer::contains_key(const HardKeys& keys, const std::string& key) {
    return keys.contains(key);
}

std::string Normalizer::value_of(const HardKeys& keys, const std::string& key) {
    auto it = keys.find(key);
    if (it == keys.end()) {
        return "";
    }
    return it->second;
}

static std::vector<char32_t> decode_codepoints(const std::string& s) {
    std::vector<char32_t> out;
    std::size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp = 0;
        int extra = 0;
        if ((c & 0x80) == 0) {
            cp = c;
            extra = 0;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            extra = 3;
        } else {
            cp = c;
            extra = 0;
        }
        for (int k = 0; k < extra && (i + 1) < s.size(); ++k) {
            ++i;
            cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
        }
        out.push_back(cp);
        ++i;
    }
    return out;
}

std::size_t estimate_tokens(const std::string& text) {
    if (text.empty()) {
        return 0;
    }
    auto cps = decode_codepoints(text);
    std::size_t count = 0;
    bool in_word = false;
    for (char32_t cp : cps) {
        if (cp == U' ' || cp == U'\n' || cp == U'\t') {
            if (in_word) {
                in_word = false;
            }
        } else if (cp >= 0x80) {
            if (in_word) {
                in_word = false;
                ++count;
            }
            ++count;
        } else {
            if (!in_word) {
                in_word = true;
                ++count;
            }
        }
    }
    return count;
}

namespace {

constexpr std::size_t kSoftDim = 64;

std::uint64_t fnv1a(const std::string& data) {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

// Feature-hashing bag-of-ngrams. Each word contributes its hash to a
// 64-dimensional embedding, plus optional bigrams and trigrams. The
// result is L2-normalized so that two documents with the same
// vocabulary produce a cosine similarity close to 1.
SoftFingerprint build_soft(const std::string& content) {
    SoftFingerprint fp;
    fp.embedding.assign(kSoftDim, 0.0);
    if (content.empty()) {
        fp.confidence = 0.0;
        return fp;
    }
    auto cps = decode_codepoints(content);
    std::vector<std::string> tokens;
    std::string current;
    auto flush = [&] {
        if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
        }
    };
    for (char32_t cp : cps) {
        if (cp == U' ' || cp == U'\n' || cp == U'\t' || cp == U'\r') {
            flush();
        } else if (cp >= 0x80) {
            flush();
            char buf[16];
            int n = std::snprintf(buf, sizeof(buf), "%08x",
                                  static_cast<unsigned>(cp));
            tokens.emplace_back(buf, static_cast<std::size_t>(n));
        } else {
            current.push_back(static_cast<char>(
                std::tolower(static_cast<int>(cp))));
        }
    }
    flush();

    auto deposit = [&](const std::string& tok, double weight) {
        std::uint64_t h = fnv1a(tok);
        std::size_t bucket = static_cast<std::size_t>(h % kSoftDim);
        int sign = (h & 1ULL) ? 1 : -1;
        fp.embedding[bucket] += weight * sign;
    };

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        deposit(tokens[i], 1.0);
        if (i + 1 < tokens.size()) {
            deposit(tokens[i] + " " + tokens[i + 1], 0.5);
        }
        if (i + 2 < tokens.size()) {
            deposit(tokens[i] + " " + tokens[i + 1] + " " + tokens[i + 2],
                    0.25);
        }
    }
    double norm = 0.0;
    for (double v : fp.embedding) {
        norm += v * v;
    }
    norm = std::sqrt(norm);
    if (norm > 0.0) {
        for (double& v : fp.embedding) {
            v /= norm;
        }
    }
    // Confidence saturates once the embedding has seen enough distinct
    // tokens to make the cosine signal meaningful.
    fp.confidence = std::min(1.0, static_cast<double>(tokens.size()) / 16.0);
    return fp;
}

} // namespace

Digest Digest::build(const std::string& type, const std::string& content) {
    Digest digest;
    digest.hard_keys_ = Normalizer::hard_keys(content);
    digest.hard_fingerprint_ = type + "\n" + join(digest.hard_keys_);
    digest.soft_ = build_soft(content);
    return digest;
}

bool Digest::matches_soft(const Digest& other, double threshold) const {
    if (soft_.embedding.size() != other.soft_.embedding.size()) {
        return false;
    }
    if (soft_.embedding.empty()) {
        return false;
    }
    double dot = 0.0;
    for (std::size_t i = 0; i < soft_.embedding.size(); ++i) {
        dot += soft_.embedding[i] * other.soft_.embedding[i];
    }
    return dot >= threshold;
}

double Digest::soft_similarity(const Digest& other) const {
    if (soft_.embedding.size() != other.soft_.embedding.size() ||
        soft_.embedding.empty()) {
        return 0.0;
    }
    double dot = 0.0;
    for (std::size_t i = 0; i < soft_.embedding.size(); ++i) {
        dot += soft_.embedding[i] * other.soft_.embedding[i];
    }
    return dot;
}

nlohmann::json Digest::to_json() const {
    nlohmann::json j;
    j["hard_fingerprint"] = hard_fingerprint_;
    j["hard_keys"] = nlohmann::json::object();
    for (const auto& [k, v] : hard_keys_) {
        j["hard_keys"][k] = v;
    }
    j["soft"]["confidence"] = soft_.confidence;
    j["soft"]["embedding"] = soft_.embedding;
    return j;
}

Digest Digest::from_json(const nlohmann::json& json) {
    Digest digest;
    digest.hard_fingerprint_ = json.value("hard_fingerprint", "");
    if (json.contains("hard_keys") && json["hard_keys"].is_object()) {
        for (auto it = json["hard_keys"].begin(); it != json["hard_keys"].end(); ++it) {
            digest.hard_keys_[it.key()] = it.value().get<std::string>();
        }
    }
    digest.soft_.confidence = json.value("soft", nlohmann::json::object())
                                  .value("confidence", 0.0);
    digest.soft_.embedding = json.value("soft", nlohmann::json::object())
                                 .value("embedding", std::vector<double>{});
    return digest;
}

} // namespace swiftagent
