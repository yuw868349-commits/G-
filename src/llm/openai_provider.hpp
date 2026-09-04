#pragma once

#include <string>
#include <httplib.h>
#include "llm/provider.hpp"

namespace praxis {

// OpenAI-compatible chat completion provider.
//
// `base_url` is the *host* of the API (no path component).  A trailing
// "/v1" or "/v1/" is automatically stripped so callers can paste a
// "https://api.openai.com/v1" style URL without it being double-
// concatenated with the default path.  `path` is appended verbatim to
// the normalized base URL and defaults to "/chat/completions".
class OpenAIProvider final : public Provider {
public:
    explicit OpenAIProvider(std::string base_url, std::string api_key,
                            std::string model = "gpt-4o-mini",
                            std::string path = "/chat/completions");

    [[nodiscard]] Result<ModelResponse> complete(const Messages& context) override;
    [[nodiscard]] std::string name() const override { return "openai"; }

    [[nodiscard]] const std::string& base_url() const noexcept { return base_url_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }

private:
    // Strip a trailing "/v1" or "/v1/" so we never end up with
    // "https://api.openai.com/v1/v1/chat/completions" when the user
    // provides the canonical OpenAI base URL.
    static std::string strip_trailing_v1(std::string url);

    std::string base_url_;
    std::string api_key_;
    std::string model_;
    std::string path_;
    httplib::Client client_;
};

} // namespace praxis
