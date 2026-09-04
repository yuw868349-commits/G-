// Backwards-compatible shim: the canonical `ToolRegistry` now lives in
// `core/registry.hpp` so the `core` library can use it without
// depending on the `tools` library. Existing callers that include
// `tools/registry.hpp` continue to work unchanged.
#pragma once

#include "core/registry.hpp"
