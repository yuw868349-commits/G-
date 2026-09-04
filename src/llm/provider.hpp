#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>
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

// A discriminated union of `Error` and `T`.  Accessing the wrong
// alternative throws rather than triggering undefined behaviour, so
// `result.value()` on a failed result is a recoverable error
// (BadExpectedAccess in spirit) rather than UB.  The implementation
// is intentionally minimal and header-only because Result is used
// pervasively across the codebase.
template <typename T>
class Result {
public:
    // Default constructor: a default-constructed Result is a *failed*
    // Result with an Internal error.  This is the safe default — a
    // caller that forgets to assign a value before reading sees an
    // error instead of dereferencing an empty variant.
    Result() : data_(std::in_place_index<0>,
                     Error{ErrorKind::Internal, "default-constructed Result"}) {}

    static Result ok(T value) { return Result(std::in_place_index<1>, std::move(value)); }
    static Result fail(Error error) { return Result(std::in_place_index<0>, std::move(error)); }

    [[nodiscard]] bool ok() const noexcept { return data_.index() == 1; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok(); }

    // Throws std::runtime_error if the result is an error.  Use
    // `value_or(default)` to recover safely when the caller cannot
    // throw, or check `ok()` first when the failure path needs to
    // inspect the error.
    [[nodiscard]] T& value() & {
        if (data_.index() != 1) {
            throw std::runtime_error(
                "Result::value() called on a failed result: " +
                std::get<0>(data_).message);
        }
        return std::get<1>(data_);
    }
    [[nodiscard]] const T& value() const& {
        if (data_.index() != 1) {
            throw std::runtime_error(
                "Result::value() called on a failed result: " +
                std::get<0>(data_).message);
        }
        return std::get<1>(data_);
    }
    [[nodiscard]] T&& value() && {
        if (data_.index() != 1) {
            throw std::runtime_error(
                "Result::value() called on a failed result: " +
                std::get<0>(data_).message);
        }
        return std::move(std::get<1>(data_));
    }

    [[nodiscard]] T value_or(T default_value) const& {
        if (data_.index() != 1) {
            return default_value;
        }
        return std::get<1>(data_);
    }
    [[nodiscard]] T value_or(T default_value) && {
        if (data_.index() != 1) {
            return default_value;
        }
        return std::move(std::get<1>(data_));
    }

    [[nodiscard]] const Error& error() const& {
        if (data_.index() != 0) {
            throw std::runtime_error(
                "Result::error() called on a successful result");
        }
        return std::get<0>(data_);
    }

private:
    template <std::size_t I, typename U>
    Result(std::in_place_index_t<I> tag, U&& v)
        : data_(tag, std::forward<U>(v)) {}

    std::variant<Error, T> data_;
};

class Provider {
public:
    virtual ~Provider() = default;
    [[nodiscard]] virtual Result<ModelResponse> complete(const Messages& context) = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

} // namespace swiftagent
