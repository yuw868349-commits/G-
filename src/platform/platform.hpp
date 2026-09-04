#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace swiftagent {

struct ProcessSpec {
    std::string command;
    std::vector<std::string> args;
    std::string working_directory;
};

struct ProcessResult {
    int exit_code{0};
    std::string stdout_output;
    std::string stderr_output;
    bool timed_out{false};
};

class Platform {
public:
    virtual ~Platform() = default;

    [[nodiscard]] virtual ProcessResult run(const ProcessSpec& spec,
                                            std::chrono::milliseconds timeout) = 0;
    [[nodiscard]] virtual std::string read_file(const std::string& path) = 0;
    virtual void write_file(const std::string& path, const std::string& content) = 0;
    [[nodiscard]] virtual bool file_exists(const std::string& path) const = 0;
    [[nodiscard]] virtual std::uint64_t file_size(const std::string& path) const = 0;
    [[nodiscard]] virtual std::vector<std::string> list_dir(const std::string& path) const = 0;
    [[nodiscard]] virtual std::string hostname() const = 0;
    [[nodiscard]] virtual std::uint64_t now_ms() const = 0;
};

std::unique_ptr<Platform> make_platform();

#ifdef _WIN32
std::unique_ptr<Platform> make_windows_platform();
#else
std::unique_ptr<Platform> make_posix_platform();
#endif

} // namespace swiftagent
