#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include "core/side_effect.hpp"

namespace fs = std::filesystem;
using namespace swiftagent;

namespace {

void write_file(const fs::path& p, const std::string& content) {
    std::ofstream out(p);
    out << content;
}

} // namespace

TEST_CASE("side effect observer reports new files") {
    auto root = fs::temp_directory_path() / "swiftagent_side_effect_test_new";
    fs::remove_all(root);
    fs::create_directories(root);
    SideEffectObserver obs;
    obs.snapshot(root);
    write_file(root / "added.txt", "hello");
    auto changed = obs.diff(root);
    CHECK(changed.size() == 1);
    fs::remove_all(root);
}

TEST_CASE("side effect observer reports modified files") {
    auto root = fs::temp_directory_path() / "swiftagent_side_effect_test_mod";
    fs::remove_all(root);
    fs::create_directories(root);
    write_file(root / "f.txt", "before");
    SideEffectObserver obs;
    obs.snapshot(root);
    write_file(root / "f.txt", "after");
    auto changed = obs.diff(root);
    CHECK(changed.size() == 1);
    fs::remove_all(root);
}

TEST_CASE("side effect observer reports removed files") {
    auto root = fs::temp_directory_path() / "swiftagent_side_effect_test_rm";
    fs::remove_all(root);
    fs::create_directories(root);
    write_file(root / "g.txt", "data");
    SideEffectObserver obs;
    obs.snapshot(root);
    fs::remove(root / "g.txt");
    auto changed = obs.diff(root);
    CHECK(changed.size() == 1);
    fs::remove_all(root);
}
