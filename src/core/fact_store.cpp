#include "core/fact_store.hpp"

#include <filesystem>
#include <cstdio>

namespace swiftagent {

namespace {

std::string fnv1a_hex(const std::string& data) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016zx%016zx",
                  hash, static_cast<std::uint64_t>(data.size()));
    return buf;
}

} // namespace

FactStore::FactStore(std::string path)
    : path_(std::move(path)) {
    if (!path_.empty()) {
        if (std::filesystem::exists(path_)) {
            std::ifstream in(path_);
            std::string line;
            while (std::getline(in, line)) {
                auto pos = line.find('\t');
                if (pos == std::string::npos) {
                    continue;
                }
                auto type = line.substr(0, pos);
                auto content = line.substr(pos + 1);
                entries_[digest_hex(type, content)] = Fact{digest_hex(type, content), type, content};
            }
        }
        out_.open(path_, std::ios::app);
    }
}

std::string FactStore::digest_hex(const std::string& type, const std::string& content) {
    return fnv1a_hex(type + "\n" + content);
}

std::string FactStore::append(const std::string& type, const std::string& content) {
    auto id = digest_hex(type, content);
    if (entries_.contains(id)) {
        return id;
    }
    entries_[id] = Fact{id, type, content};
    if (out_.is_open()) {
        out_ << type << '\t' << content << '\n';
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

} // namespace swiftagent
