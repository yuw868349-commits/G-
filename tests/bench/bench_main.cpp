#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "benchwork.hpp"

using namespace swiftagent;

namespace {

void run_one(const std::string& name, std::size_t n, bool parallel) {
    BenchResult r;
    if (name == "reorganize") {
        r = BenchHarness::file_reorganization(n, parallel);
    } else if (name == "gather") {
        r = BenchHarness::data_gathering(n, parallel);
    } else if (name == "install") {
        r = BenchHarness::dependency_install(n, parallel);
    } else {
        std::cerr << "unknown bench: " << name << "\n";
        std::exit(1);
    }
    std::cout << name << " n=" << n
              << " parallel=" << (parallel ? "yes" : "no")
              << " elapsed_ms=" << r.elapsed_ms
              << " baseline_ms=" << r.baseline_ms
              << " speedup_x=" << r.speedup()
              << " tool_invocations=" << r.tool_invocations
              << "\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string name = args.empty() ? "reorganize" : args[0];
    std::size_t n = args.size() > 1 ? std::stoul(args[1]) : 32;
    bool parallel = args.size() <= 2 || args[2] != "serial";
    run_one(name, n, parallel);
    run_one(name, n, false);
    return 0;
}
