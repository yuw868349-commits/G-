#include "llm/openai_provider.hpp"

#include <nlohmann/json.hpp>
#include "core/error.hpp"

namespace swiftagent {

namespace {

// Trim any trailing slashes so we can compare against "/v1" without
// worrying about whether the user added an extra one.
std::string rtrim_slashes(const std::string& s) {
    std::size_t end = s.size();
    while (end > 0 && s[end - 1] == '/') {
        --end;
    }
    return s.substr(0, end);
}

}  // namespace

std::string OpenAIProvider::strip_trailing_v1(std::string url) {
    auto trimmed = rtrim_slashes(url);
    if (trimmed.size() >= 3 &&
        trimmed.compare(trimmed.size() - 3, 3, "/v1") == 0) {
        return rtrim_slashes(trimmed.substr(0, trimmed.size() - 3));
    }
    return url;
}

OpenAIProvider::OpenAIProvider(std::string base_url, std::string api_key,
                               std::string model, std::string path)
    : base_url_(strip_trailing_v1(std::move(base_url))),
      api_key_(std::move(api_key)),
      model_(std::move(model)),
      path_(std::move(path)),
      client_(base_url_) {
    client_.set_connection_timeout(10, 0);
    client_.set_read_timeout(120, 0);
}

Result<ModelResponse> OpenAIProvider::complete(const Messages& context) {
    nlohmann::json body;
    body["model"] = model_;
    body["messages"] = nlohmann::json::array();
    for (const auto& msg : context) {
        nlohmann::json m;
        m["role"] = msg.role;
        m["content"] = msg.content;
        if (msg.tool_calls) {
            m["tool_calls"] = *msg.tool_calls;
        }
        body["messages"].push_back(std::move(m));
    }

    auto res = client_.Post(path_, body.dump(), "application/json");
    if (!res) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::ProviderFailure, "http request failed"});
    }
    if (res->status != 200) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::ProviderFailure, "http status " + std::to_string(res->status)});
    }

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(res->body);
    } catch (const nlohmann::json::exception&) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::ProviderFailure, "invalid json response"});
    }

    ModelResponse response;
    response.raw = parsed;
    if (!parsed.contains("choices") || !parsed["choices"].is_array() ||
        parsed["choices"].empty()) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::ProviderFailure, "response missing choices"});
    }
    const auto& message = parsed["choices"][0]["message"];
    response.outcome.plan = message.value("content", "");
    auto tool_calls = message.value("tool_calls", nlohmann::json::array());
    if (!tool_calls.is_array()) {
        tool_calls = nlohmann::json::array();
    }
    response.outcome.has_tool_use = tool_calls.is_array() && !tool_calls.empty();
    response.outcome.tool_count = static_cast<std::uint32_t>(tool_calls.size());
    return Result<ModelResponse>::ok(std::move(response));
}

} // namespace swiftagent
