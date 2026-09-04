#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include "platform/platform.hpp"

#if !defined(_WIN32)

using namespace swiftagent;

TEST_CASE("platform run honours timeout for silent children") {
    auto plat = make_platform();
    ProcessSpec spec;
    // `sleep 5` produces no output and would block forever on a
    // read()-based implementation.  A 100ms timeout must return
    // promptly with timed_out=true.
    spec.command = "/bin/sh";
    spec.args = {"-c", "sleep 5"};
    auto t0 = std::chrono::steady_clock::now();
    auto result = plat->run(spec, std::chrono::milliseconds(100));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    CHECK(result.timed_out);
    CHECK(elapsed < 2000);  // generous bound; in practice < 200 ms.
}

TEST_CASE("platform run returns successfully for fast children") {
    auto plat = make_platform();
    ProcessSpec spec;
    spec.command = "/bin/sh";
    spec.args = {"-c", "echo hello"};
    auto result = plat->run(spec, std::chrono::milliseconds(5000));
    CHECK_FALSE(result.timed_out);
    CHECK(result.exit_code == 0);
    CHECK(result.stdout_output.find("hello") != std::string::npos);
}

#endif
