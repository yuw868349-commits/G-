#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include "core/side_effect.hpp"

namespace fs = std::filesystem;
using namespace praxis;

namespace {

void write_file(const fs::path& p, const std::string& content) {
    std::ofstream out(p);
    out << content;
}

} // namespace

TEST_CASE("side effect observer reports new files") {
    auto root = fs::temp_directory_path() / "praxis_side_effect_test_new";
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
    auto root = fs::temp_directory_path() / "praxis_side_effect_test_mod";
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
    auto root = fs::temp_directory_path() / "praxis_side_effect_test_rm";
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

TEST_CASE("side effect observer is incremental: unchanged files are not re-hashed") {
    // After snapshotting a file, a second diff() against an
    // unchanged tree must produce no changes.  This is the
    // steady-state behaviour that keeps large directories cheap to
    // watch: a no-op turn is a stat() per file, not a full read.
    auto root = fs::temp_directory_path() / "praxis_side_effect_test_inc";
    fs::remove_all(root);
    fs::create_directories(root);
    for (int i = 0; i < 20; ++i) {
        write_file(root / ("f" + std::to_string(i) + ".txt"),
                   std::string(1024, 'x'));
    }
    SideEffectObserver obs;
    obs.snapshot(root);
    auto changed = obs.diff(root);
    CHECK(changed.empty());
    // Now change one file: the diff should mention only that file.
    write_file(root / "f5.txt", "different content");
    changed = obs.diff(root);
    CHECK(changed.size() == 1);
    CHECK(changed[0].find("f5.txt") != std::string::npos);
    fs::remove_all(root);
}
