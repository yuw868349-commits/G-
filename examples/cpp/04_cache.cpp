// Use the dependency-aware cache directly.
//
// Build: cmake --build build --target example_cache
// Run:   ./build/example_cache

#include <iostream>

#include "core/cache.hpp"

int main()
{
    using namespace praxis;

    Cache cache;

    cache.put("answer", {{"value", 42}}, {"file:///a.txt", "file:///b.txt"});
    cache.put("answer-cacheable", {{"value", 7}}, {"file:///c.txt"}, /*cacheable=*/false);

    auto hit = cache.get("answer", {"file:///a.txt", "file:///b.txt"});
    if (hit)
    {
        std::cout << "hit: " << hit->dump() << "\n";
    }
    else
    {
        std::cout << "miss\n";
    }

    auto blocked = cache.get("answer-cacheable", {"file:///c.txt"});
    std::cout << "non-cacheable returned: " << (blocked ? "yes" : "no") << "\n";

    cache.invalidate_dependency("file:///a.txt");
    auto after_invalidate = cache.get("answer", {"file:///a.txt", "file:///b.txt"});
    std::cout << "after invalidate: " << (after_invalidate ? "hit" : "miss") << "\n";

    std::cout << "hits=" << cache.hits() << " misses=" << cache.misses() << "\n";
    return 0;
}
