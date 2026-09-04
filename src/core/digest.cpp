#include "core/digest.hpp"

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

Digest Digest::build(const std::string& type, const std::string& content) {
    Digest digest;
    digest.hard_keys_ = Normalizer::hard_keys(content);
    digest.hard_fingerprint_ = type + "\n" + join(digest.hard_keys_);
    digest.soft_.embedding = {0.0};
    digest.soft_.confidence = digest.hard_keys_.empty() ? 0.3 : 1.0;
    return digest;
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
