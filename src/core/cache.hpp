#pragma once

#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace swiftagent {

struct CacheEntry {
    std::string key;
    nlohmann::json value;
    std::vector<std::string> dependencies;
    std::unordered_map<std::string, std::uint64_t> dep_versions;
    std::uint64_t hit_count{0};
    bool cacheable{true};
};

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

private:
    [[nodiscard]] bool is_valid(const CacheEntry& entry,
                                const std::vector<std::string>& current) const;

    mutable std::mutex mtx_;
    std::unordered_map<std::string, CacheEntry> entries_;
    std::unordered_map<std::string, std::uint64_t> dep_versions_;
    mutable std::size_t hits_{0};
    mutable std::size_t misses_{0};
};

} // namespace swiftagent
