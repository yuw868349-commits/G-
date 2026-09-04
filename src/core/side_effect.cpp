#include "core/side_effect.hpp"

#include <fstream>
#include <sstream>

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

} // namespace

std::string SideEffectObserver::hash_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return "";
    }
    std::stringstream ss;
    ss << in.rdbuf();
    auto bytes = ss.str();
    auto h = fnv1a(bytes);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016zx", h);
    return std::string(buf);
}

std::vector<std::filesystem::path> SideEffectObserver::walk(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> out;
    if (!std::filesystem::exists(root)) {
        return out;
    }
    for (auto it = std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied);
         it != std::filesystem::recursive_directory_iterator();
         ++it) {
        if (it->is_regular_file()) {
            out.push_back(it->path());
        }
    }
    return out;
}

void SideEffectObserver::snapshot(const std::filesystem::path& root) {
    before_.clear();
    for (const auto& path : walk(root)) {
        FileSnapshot snap;
        snap.existed = true;
        snap.mtime = std::filesystem::last_write_time(path);
        snap.size = static_cast<std::uint64_t>(std::filesystem::file_size(path));
        snap.hash = hash_file(path);
        before_[path.string()] = snap;
    }
}

std::vector<std::string> SideEffectObserver::diff(const std::filesystem::path& root) const {
    std::vector<std::string> changed;
    std::unordered_set<std::string> seen;
    for (const auto& path : walk(root)) {
        seen.insert(path.string());
        auto it = before_.find(path.string());
        if (it == before_.end()) {
            changed.push_back(path.string());
            continue;
        }
        if (it->second.size != static_cast<std::uint64_t>(std::filesystem::file_size(path)) ||
            it->second.hash != hash_file(path)) {
            changed.push_back(path.string());
        }
    }
    for (const auto& [p, _] : before_) {
        if (!seen.contains(p)) {
            changed.push_back(p);
        }
    }
    return changed;
}

void SideEffectObserver::clear() {
    before_.clear();
}

bool SideEffectObserver::has_hash(const std::string& path) const {
    return before_.contains(path);
}

std::optional<std::string> SideEffectObserver::hash_of(const std::string& path) const {
    auto it = before_.find(path);
    if (it == before_.end()) {
        return std::nullopt;
    }
    return it->second.hash;
}

} // namespace swiftagent
