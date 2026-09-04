#include "core/cache.hpp"

#include <algorithm>

namespace swiftagent {

bool Cache::is_valid(const CacheEntry& entry,
                     const std::vector<std::string>& current) const {
    if (!entry.cacheable) {
        return false;
    }
    if (current.size() != entry.dependencies.size()) {
        return false;
    }
    for (std::size_t i = 0; i < current.size(); ++i) {
        if (current[i] != entry.dependencies[i]) {
            return false;
        }
    }
    for (const auto& [dep, version] : entry.dep_versions) {
        auto it = dep_versions_.find(dep);
        if (it == dep_versions_.end() || it->second != version) {
            return false;
        }
    }
    return true;
}

void Cache::put(const std::string& key, const nlohmann::json& value,
                const std::vector<std::string>& dependencies, bool cacheable) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        it->second.value = value;
        it->second.dependencies = dependencies;
        it->second.cacheable = cacheable;
        it->second.dep_versions.clear();
        for (const auto& d : dependencies) {
            auto v = dep_versions_.find(d);
            if (v == dep_versions_.end()) {
                dep_versions_[d] = 1;
                it->second.dep_versions[d] = 1;
            } else {
                it->second.dep_versions[d] = v->second;
            }
        }
        return;
    }
    CacheEntry entry{key, value, dependencies, {}, 0, cacheable};
    for (const auto& d : dependencies) {
        auto v = dep_versions_.find(d);
        if (v == dep_versions_.end()) {
            dep_versions_[d] = 1;
            entry.dep_versions[d] = 1;
        } else {
            entry.dep_versions[d] = v->second;
        }
    }
    entries_[key] = std::move(entry);
}

std::optional<nlohmann::json>
Cache::get(const std::string& key, const std::vector<std::string>& current) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = entries_.find(key);
    if (it == entries_.end()) {
        ++misses_;
        return std::nullopt;
    }
    if (!is_valid(it->second, current)) {
        ++misses_;
        return std::nullopt;
    }
    ++hits_;
    ++const_cast<std::uint64_t&>(it->second.hit_count);
    return it->second.value;
}

void Cache::invalidate_dependency(const std::string& dep) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = dep_versions_.find(dep);
    if (it == dep_versions_.end()) {
        dep_versions_[dep] = 1;
    } else {
        ++it->second;
    }
}

void Cache::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    entries_.clear();
    dep_versions_.clear();
}

std::size_t Cache::size() const noexcept {
    return entries_.size();
}

} // namespace swiftagent
