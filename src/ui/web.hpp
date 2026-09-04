#pragma once

#include <httplib.h>
#include <memory>
#include "core/orchestrator.hpp"
#include "ui/cli.hpp"

namespace praxis {

int run_web(const CliOptions& opts);

} // namespace praxis
