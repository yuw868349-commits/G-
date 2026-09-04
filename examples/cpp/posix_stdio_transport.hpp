// Bidirectional stdio MCP transport via pipe/fork/exec (POSIX).
// Used by the example_mcp binary; the real codebase can use any
// JsonRpcTransport implementation.

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mcp/json_rpc.hpp"

namespace example {

class PosixStdioTransport final : public swiftagent::JsonRpcTransport
{
public:
    explicit PosixStdioTransport(const std::string& command)
    {
        int in_pipe[2];
        int out_pipe[2];
        if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0)
        {
            throw std::runtime_error("pipe() failed");
        }

        pid_ = fork();
        if (pid_ < 0)
        {
            throw std::runtime_error("fork() failed");
        }

        if (pid_ == 0)
        {
            // child: stdin <- in_pipe[0], stdout -> out_pipe[1]
            dup2(in_pipe[0], STDIN_FILENO);
            dup2(out_pipe[1], STDOUT_FILENO);
            close(in_pipe[1]);
            close(out_pipe[0]);
            close(in_pipe[0]);
            close(out_pipe[1]);
            execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }

        // parent: read from out_pipe[0], write to in_pipe[1]
        close(in_pipe[0]);
        close(out_pipe[1]);
        read_fd_ = out_pipe[0];
        write_fd_ = in_pipe[1];
    }

    ~PosixStdioTransport() override
    {
        if (read_fd_ >= 0)
        {
            close(read_fd_);
        }
        if (write_fd_ >= 0)
        {
            close(write_fd_);
        }
        if (pid_ > 0)
        {
            int status = 0;
            waitpid(pid_, &status, 0);
        }
    }

    void send(const std::string& payload) override
    {
        const char* p = payload.data();
        std::size_t left = payload.size();
        while (left > 0)
        {
            ssize_t n = ::write(write_fd_, p, left);
            if (n <= 0)
            {
                throw std::runtime_error("stdio write failed");
            }
            p += n;
            left -= static_cast<std::size_t>(n);
        }
        const char nl = '\n';
        [[maybe_unused]] ssize_t _ = ::write(write_fd_, &nl, 1);
    }

    std::string receive() override
    {
        if (!line_.empty())
        {
            std::string out = std::move(line_);
            line_.clear();
            return out;
        }
        char buf[4096];
        ssize_t n = ::read(read_fd_, buf, sizeof(buf));
        if (n <= 0)
        {
            return {};
        }
        buffer_.append(buf, static_cast<std::size_t>(n));
        return pop_line();
    }

    bool alive() const override { return read_fd_ >= 0 && pid_ > 0; }

private:
    std::string pop_line()
    {
        auto pos = buffer_.find('\n');
        if (pos == std::string::npos)
        {
            std::string out = std::move(buffer_);
            buffer_.clear();
            return out;
        }
        std::string out = buffer_.substr(0, pos);
        buffer_.erase(0, pos + 1);
        return out;
    }

    pid_t pid_{-1};
    int read_fd_{-1};
    int write_fd_{-1};
    std::string buffer_;
    std::string line_;
};

} // namespace example
