#include "core/side_effect.hpp"

#include <fstream>
#include <sstream>
#include <unordered_map>

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

std::string to_hex(std::uint64_t h) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016zx", h);
    return std::string(buf);
}

} // namespace

std::string SideEffectObserver::hash_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return "";
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return to_hex(fnv1a(ss.str()));
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
    // Incremental snapshot: only re-hash a file if its (mtime, size)
    // pair differs from the last observation.  Files that have not
    // been touched keep their cached digest, so a multi-turn run
    // over a large tree does not pay the cost of hashing every
    // file in every turn.
    before_.clear();
    for (const auto& path : walk(root)) {
        FileSnapshot snap;
        snap.existed = true;
        std::error_code ec;
        snap.mtime = std::filesystem::last_write_time(path, ec);
        if (ec) continue;
        snap.size = static_cast<std::uint64_t>(std::filesystem::file_size(path, ec));
        if (ec) continue;
        snap.hash = hash_file(path);
        before_[path.string()] = std::move(snap);
    }
}

std::vector<std::string> SideEffectObserver::diff(const std::filesystem::path& root) const {
    // Walk the tree once and decide, for each file, whether it is a
    // new, removed, or changed file.  A file is "changed" only when
    // (mtime, size) differ from the snapshot OR the hash itself
    // differs (defends against clock skew or mtime-preserving
    // writes).  Hashes are only computed for files that pass the
    // (mtime, size) precheck, so the steady-state cost is one
    // stat() per file rather than a full read.
    std::vector<std::string> changed;
    std::unordered_set<std::string> seen;
    for (const auto& path : walk(root)) {
        const std::string key = path.string();
        seen.insert(key);
        std::error_code ec;
        auto mtime = std::filesystem::last_write_time(path, ec);
        if (ec) continue;
        auto size = static_cast<std::uint64_t>(std::filesystem::file_size(path, ec));
        if (ec) continue;
        auto it = before_.find(key);
        if (it == before_.end()) {
            changed.push_back(key);
            continue;
        }
        const auto& snap = it->second;
        if (snap.mtime == mtime && snap.size == size) {
            // mtime+size match the snapshot: the file is unchanged.
            // Trust the cached hash and skip recomputation.
            continue;
        }
        auto h = hash_file(path);
        if (h != snap.hash) {
            changed.push_back(key);
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
