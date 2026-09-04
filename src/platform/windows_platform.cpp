#include "platform/platform.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace swiftagent {

class WindowsPlatform final : public Platform {
public:
    ProcessResult run(const ProcessSpec& spec,
                      std::chrono::milliseconds timeout) override {
        // The original implementation called blocking ReadFile in a
        // loop and only checked the deadline *after* the call
        // returned, so a child that produced no output would wedge
        // the call forever.  Use overlapped I/O + WaitForSingleObject
        // so the timeout can be observed even when the child is
        // silent.  The process is terminated via TerminateProcess if
        // the deadline is reached.
        std::string cmd = spec.command;
        for (const auto& a : spec.args) {
            cmd += " ";
            cmd += "\"";
            cmd += a;
            cmd += "\"";
        }
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        HANDLE read_pipe = nullptr;
        HANDLE write_pipe = nullptr;
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;
        if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
            return ProcessResult{-1, "", "create pipe failed", false};
        }
        si.hStdError = write_pipe;
        si.hStdOutput = write_pipe;
        si.dwFlags |= STARTF_USESTDHANDLES;
        std::string full = cmd + " 2>&1";
        if (!CreateProcessA(nullptr, full.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(read_pipe);
            CloseHandle(write_pipe);
            return ProcessResult{-1, "", "create process failed", false};
        }
        CloseHandle(write_pipe);

        std::string out;
        char buf[4096];
        HANDLE handles[2] = {pi.hProcess, read_pipe};
        bool timed_out = false;
        auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                timed_out = true;
                break;
            }
            auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     deadline - now)
                                     .count();
            if (remaining_ms < 1) remaining_ms = 1;
            DWORD wait = static_cast<DWORD>(remaining_ms);
            DWORD which = WaitForMultipleObjects(2, handles, FALSE, wait);
            if (which == WAIT_TIMEOUT) {
                // Either the process or the pipe is silent; loop
                // back to re-check the deadline.
                continue;
            }
            if (which == WAIT_FAILED) {
                break;
            }
            if (which == WAIT_OBJECT_0) {
                // Process exited; drain remaining pipe data.
                for (;;) {
                    DWORD read_bytes = 0;
                    if (!ReadFile(read_pipe, buf, sizeof(buf),
                                  &read_bytes, nullptr) || read_bytes == 0) {
                        break;
                    }
                    out.append(buf, read_bytes);
                }
                break;
            }
            if (which == WAIT_OBJECT_0 + 1) {
                DWORD read_bytes = 0;
                if (!ReadFile(read_pipe, buf, sizeof(buf),
                              &read_bytes, nullptr) || read_bytes == 0) {
                    // Pipe closed: process is exiting.
                    continue;
                }
                out.append(buf, read_bytes);
            }
        }
        if (timed_out) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(read_pipe);
            return ProcessResult{-1, out, "timeout", true};
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(read_pipe);
        return ProcessResult{static_cast<int>(exit_code), out, "", false};
    }

    std::string read_file(const std::string& path) override {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return "";
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    void write_file(const std::string& path, const std::string& content) override {
        std::ofstream out(path, std::ios::binary);
        out << content;
    }

    bool file_exists(const std::string& path) const override {
        DWORD attr = GetFileAttributesA(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
    }

    std::uint64_t file_size(const std::string& path) const override {
        WIN32_FILE_ATTRIBUTE_DATA info{};
        if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &info)) {
            return 0;
        }
        return (static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) |
               info.nFileSizeLow;
    }

    std::vector<std::string> list_dir(const std::string& path) const override {
        std::vector<std::string> out;
        WIN32_FIND_DATAA data{};
        std::string pattern = path + "\\*";
        HANDLE h = FindFirstFileA(pattern.c_str(), &data);
        if (h == INVALID_HANDLE_VALUE) {
            return out;
        }
        do {
            if (std::string(data.cFileName) == "." || std::string(data.cFileName) == "..") {
                continue;
            }
            out.push_back(data.cFileName);
        } while (FindNextFileA(h, &data));
        FindClose(h);
        return out;
    }

    std::string hostname() const override {
        char buf[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(buf);
        if (!GetComputerNameA(buf, &size)) {
            return "unknown";
        }
        return std::string(buf, size);
    }

    std::uint64_t now_ms() const override {
        using namespace std::chrono;
        return static_cast<std::uint64_t>(
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
    }
};

std::unique_ptr<Platform> make_windows_platform() {
    return std::make_unique<WindowsPlatform>();
}

std::unique_ptr<Platform> make_platform() {
    return make_windows_platform();
}

} // namespace swiftagent
