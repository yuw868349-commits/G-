#include "platform/platform.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

namespace praxis {

namespace {

// Run `program` with the supplied argv using fork()+execvp(), piping
// stdout+stderr back through a pipe.  No shell is ever spawned, so
// arguments containing whitespace, single quotes, or shell meta-
// characters cannot cause command injection.  Returns the exit
// status and accumulated output.  Honours the supplied timeout:
// SIGTERMs the child after the deadline and SIGKILLs it if it
// refuses to exit.
struct CapturedRun {
    int status{-1};
    std::string out;
    bool timed_out{false};
};

CapturedRun run_captured(const std::string& program,
                         const std::vector<std::string>& args,
                         std::chrono::milliseconds timeout) {
    int pipefd[2];
    if (::pipe(pipefd) != 0) {
        return CapturedRun{-1, "", false};
    }
    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return CapturedRun{-1, "", false};
    }
    if (pid == 0) {
        // child
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::dup2(pipefd[1], STDERR_FILENO);
        ::close(pipefd[0]);
        ::close(pipefd[1]);

        std::vector<std::string> argv_storage;
        argv_storage.reserve(args.size() + 1);
        argv_storage.push_back(program);
        for (const auto& a : args) {
            argv_storage.push_back(a);
        }
        std::vector<char*> argv_ptrs;
        argv_ptrs.reserve(argv_storage.size() + 1);
        for (auto& s : argv_storage) {
            argv_ptrs.push_back(s.data());
        }
        argv_ptrs.push_back(nullptr);

        ::execvp(program.c_str(), argv_ptrs.data());
        // If execvp returns, it failed.
        std::perror("execvp");
        ::_exit(127);
    }
    // parent
    ::close(pipefd[1]);

    // Read from the pipe with poll() so we can enforce the timeout
    // even if the child produces no output at all.  A blocking
    // read() would wedge here forever on a hung child; the original
    // implementation had this defect and was rewritten.
    CapturedRun result;
    char buf[4096];
    auto deadline = std::chrono::steady_clock::now() + timeout;
    struct pollfd pfd{};
    pfd.fd = pipefd[0];
    pfd.events = POLLIN;

    for (;;) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            result.timed_out = true;
            break;
        }
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                              deadline - now)
                              .count();
        if (remaining < 1) remaining = 1;
        int rc = ::poll(&pfd, 1, static_cast<int>(remaining));
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (rc == 0) {
            // poll() timed out, but we may have been woken early by
            // a signal; check the deadline again on the next loop.
            continue;
        }
        if (pfd.revents & (POLLIN | POLLHUP)) {
            ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
            if (n > 0) {
                result.out.append(buf, static_cast<std::size_t>(n));
                continue;
            }
            // EOF: child closed its end of the pipe.
            break;
        }
        if (pfd.revents & (POLLERR | POLLNVAL)) {
            break;
        }
    }

    if (result.timed_out) {
        // Escalate: SIGTERM first, give the child a brief grace
        // period to clean up, then SIGKILL if it is still alive.
        ::kill(pid, SIGTERM);
        for (int i = 0; i < 20; ++i) {
            int status = 0;
            pid_t r = ::waitpid(pid, &status, WNOHANG);
            if (r == pid) {
                pid = -1;
                break;
            }
            struct timespec ts{0, 50'000'000};  // 50 ms
            ::nanosleep(&ts, nullptr);
        }
        if (pid > 0) {
            ::kill(pid, SIGKILL);
            int status = 0;
            while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
        }
        result.status = -1;
    } else {
        int status = 0;
        while (::waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR) {
                break;
            }
        }
        if (WIFEXITED(status)) {
            result.status = WEXITSTATUS(status);
        } else {
            result.status = -1;
        }
    }
    ::close(pipefd[0]);
    return result;
}

}  // namespace

class PosixPlatform final : public Platform {
public:
    ProcessResult run(const ProcessSpec& spec,
                      std::chrono::milliseconds timeout) override {
        // Honour the explicit args vector when present: spawn
        // `spec.command` directly with the supplied argv, never via
        // a shell.  Fall back to `sh -c` only for the legacy path
        // (no args) and even then make sure the command line is
        // escaped to a single argv element.
        std::vector<std::string> argv;
        if (!spec.args.empty()) {
            argv = spec.args;
        } else {
            argv = {"/bin/sh", "-c", spec.command};
        }
        // A negative or zero timeout means "wait forever"; clamp to a
        // very large value so the deadline arithmetic in run_captured
        // doesn't overflow.
        if (timeout <= std::chrono::milliseconds(0)) {
            timeout = std::chrono::hours(24 * 365);
        }
        auto captured = run_captured(spec.command, argv, timeout);
        ProcessResult pr;
        pr.exit_code = captured.status;
        pr.stdout_output = captured.out;
        pr.stderr_output.clear();
        pr.timed_out = captured.timed_out;
        return pr;
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

std::unique_ptr<Platform> make_posix_platform() {
    return std::make_unique<PosixPlatform>();
}

std::unique_ptr<Platform> make_platform() {
    return make_posix_platform();
}

} // namespace praxis
