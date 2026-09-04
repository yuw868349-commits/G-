#pragma once

#include <string>

namespace praxis {

enum class ErrorKind {
    None,
    ProviderFailure,
    ToolFailed,
    Timeout,
    Validation,
    BudgetExhausted,
    Cancelled,
    Internal
};

struct Error {
    ErrorKind kind{ErrorKind::None};
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return kind == ErrorKind::None; }
    [[nodiscard]] static Error none() noexcept { return {}; }
};

} // namespace praxis
