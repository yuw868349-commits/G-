#include "core/fact_store.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace swiftagent {

namespace {

std::uint64_t fnv1a(const std::string& data) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Escape control characters that would otherwise break the
// newline-delimited, tab-separated on-disk format:
//
//   '\n'  ->  literal backslash + n
//   '\r'  ->  literal backslash + r
//   '\t'  ->  literal backslash + t
//   '\\'  ->  literal backslash + backslash
std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            switch (next) {
                case '\\': out += '\\'; ++i; break;
                case 'n':  out += '\n'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                default:   out += s[i];      break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

}  // namespace

FactStore::FactStore(std::string path) : path_(std::move(path)) {
    if (!path_.empty()) {
        if (auto parent = std::filesystem::path(path_).parent_path();
            !parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
        }
        if (std::filesystem::exists(path_)) {
            std::ifstream in(path_);
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                auto pos = line.find('\t');
                if (pos == std::string::npos) {
                    continue;
                }
                auto type = unescape(line.substr(0, pos));
                auto content = unescape(line.substr(pos + 1));
                auto id = digest_hex(type, content);
                entries_[id] = Fact{id, std::move(type), std::move(content)};
            }
        }
        // Open with `trunc` semantics by truncating any leftover file
        // (we just re-read it above) and then re-open in append mode so
        // concurrent appends are atomic w.r.t. each other.
        out_.open(path_, std::ios::app);
    }
}

FactStore::~FactStore() {
    flush();
    if (out_.is_open()) {
        out_.close();
    }
}

std::string FactStore::digest_hex(const std::string& type, const std::string& content) {
    std::string combined;
    combined.reserve(type.size() + 1 + content.size());
    combined.append(type);
    combined.push_back('\n');
    combined.append(content);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%016zx%016zx",
                  fnv1a(combined),
                  static_cast<std::uint64_t>(combined.size()));
    return buf;
}

std::string FactStore::append(const std::string& type, const std::string& content) {
    auto id = digest_hex(type, content);
    if (entries_.contains(id)) {
        return id;
    }
    entries_[id] = Fact{id, type, content};
    if (out_.is_open()) {
        out_ << escape(type) << '\t' << escape(content) << '\n';
        out_.flush();
    }
    return id;
}

std::optional<Fact> FactStore::get(const std::string& id) const {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void FactStore::flush() {
    if (out_.is_open()) {
        out_.flush();
    }
}

}  // namespace swiftagent
