#include <cstdlib>
#include <exception>
#include <iostream>

#include "ui/cli.hpp"
#include "ui/web.hpp"

int main(int argc, char** argv) {
    try {
        auto opts = swiftagent::parse_cli(argc, argv);
        if (opts.use_web) {
            return swiftagent::run_web(opts);
        }
        if (opts.task.empty()) {
            std::cout << "SwiftAgent CLI - pass a task or use --help\n";
            return 0;
        }
        return swiftagent::run_cli(opts);
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 2;
    }
}
