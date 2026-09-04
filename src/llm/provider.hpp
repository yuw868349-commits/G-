#pragma once

#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "core/error.hpp"
#include "core/types.hpp"

namespace swiftagent {

struct Message {
    std::string role;
    std::string content;
    std::optional<nlohmann::json> tool_calls;
};

using Messages = std::vector<Message>;

struct ModelResponse {
    TurnOutcome outcome;
    nlohmann::json raw;
};

template <typename T>
struct Result {
    std::optional<T> value_;
    std::optional<Error> error_;

    static Result ok(T value) { return Result{std::move(value), std::nullopt}; }
    static Result fail(Error error) { return Result{std::nullopt, std::move(error)}; }

    [[nodiscard]] bool ok() const noexcept { return value_.has_value(); }
    [[nodiscard]] T& value() { return *value_; }
    [[nodiscard]] const T& value() const { return *value_; }
    [[nodiscard]] const Error& error() const { return *error_; }
};

class Provider {
public:
    virtual ~Provider() = default;
    [[nodiscard]] virtual Result<ModelResponse> complete(const Messages& context) = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

} // namespace swiftagent
