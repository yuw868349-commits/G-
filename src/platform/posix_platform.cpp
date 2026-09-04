#include "platform/platform.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace swiftagent {

class PosixPlatform final : public Platform {
public:
    ProcessResult run(const ProcessSpec& spec,
                      std::chrono::milliseconds timeout) override {
        std::string cmd = spec.command;
        for (const auto& a : spec.args) {
            cmd += " ";
            cmd += "'";
            cmd += a;
            cmd += "'";
        }
        cmd += " 2>&1";
        FILE* pipe = ::popen(cmd.c_str(), "r");
        if (!pipe) {
            return ProcessResult{-1, "", "popen failed", false};
        }
        std::string out;
        char buf[4096];
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            if (std::chrono::steady_clock::now() >= deadline) {
                ::pclose(pipe);
                return ProcessResult{-1, out, "timeout", true};
            }
            if (!std::fgets(buf, sizeof(buf), pipe)) {
                break;
            }
            out += buf;
        }
        int status = ::pclose(pipe);
        int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return ProcessResult{code, out, "", false};
    }

    std::string read_file(const std::string& path) override {
        std::ifstream in(path);
        if (!in) {
            return "";
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    void write_file(const std::string& path, const std::string& content) override {
        std::ofstream out(path);
        out << content;
    }

    bool file_exists(const std::string& path) const override {
        struct stat st{};
        return ::stat(path.c_str(), &st) == 0;
    }

    std::uint64_t file_size(const std::string& path) const override {
        struct stat st{};
        if (::stat(path.c_str(), &st) != 0) {
            return 0;
        }
        return static_cast<std::uint64_t>(st.st_size);
    }

    std::vector<std::string> list_dir(const std::string& path) const override {
        std::vector<std::string> out;
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            out.push_back(entry.path().filename().string());
        }
        return out;
    }

    std::string hostname() const override {
        utsname info{};
        if (uname(&info) != 0) {
            return "unknown";
        }
        return info.nodename;
    }

    std::uint64_t now_ms() const override {
        using namespace std::chrono;
        return static_cast<std::uint64_t>(
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
    }
};

std::unique_ptr<Platform> make_platform() {
    return std::make_unique<PosixPlatform>();
}

} // namespace swiftagent
