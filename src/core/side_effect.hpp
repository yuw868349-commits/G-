#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace swiftagent {

struct FileSnapshot {
    std::filesystem::file_time_type mtime{};
    std::uint64_t size{0};
    std::string hash;
    bool existed{false};
};

class SideEffectObserver {
public:
    void snapshot(const std::filesystem::path& root);
    [[nodiscard]] std::vector<std::string> diff(const std::filesystem::path& root) const;
    void clear();

    [[nodiscard]] bool has_hash(const std::string& path) const;
    [[nodiscard]] std::optional<std::string> hash_of(const std::string& path) const;

private:
    [[nodiscard]] static std::string hash_file(const std::filesystem::path& path);
    [[nodiscard]] static std::vector<std::filesystem::path> walk(const std::filesystem::path& root);

    std::unordered_map<std::string, FileSnapshot> before_;
};

} // namespace swiftagent
