#pragma once

#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace praxis {

struct CacheEntry {
    std::string key;
    nlohmann::json value;
    std::vector<std::string> dependencies;
    std::unordered_map<std::string, std::uint64_t> dep_versions;
    bool cacheable{true};
};

// Cache of model/tool results keyed by `(tool_name, args)`.
//
// Hit/miss statistics live in a separate map on the Cache itself
// rather than on the CacheEntry.  This keeps the value returned by
// `get()` free of mutable state and lets `get()` stay `const`
// without a `const_cast` shim.
class Cache {
public:
    void put(const std::string& key, const nlohmann::json& value,
             const std::vector<std::string>& dependencies,
             bool cacheable = true);
    [[nodiscard]] std::optional<nlohmann::json>
    get(const std::string& key, const std::vector<std::string>& current_dependencies) const;
    void invalidate_dependency(const std::string& dep);
    void clear();
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t hits() const noexcept { return hits_; }
    [[nodiscard]] std::size_t misses() const noexcept { return misses_; }
    [[nodiscard]] std::size_t hits_for(const std::string& key) const;

private:
    [[nodiscard]] bool is_valid(const CacheEntry& entry,
                                const std::vector<std::string>& current) const;

    mutable std::mutex mtx_;
    std::unordered_map<std::string, CacheEntry> entries_;
    std::unordered_map<std::string, std::uint64_t> dep_versions_;
    // Statistics: hit/miss counts are written from `get()`, which
    // is logically const but mutates the counters.  The mutex
    // protects both the entries and the counters.
    mutable std::unordered_map<std::string, std::uint64_t> hits_per_key_;
    mutable std::size_t hits_{0};
    mutable std::size_t misses_{0};
};

} // namespace praxis
