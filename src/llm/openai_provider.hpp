#pragma once

#include <string>
#include <httplib.h>
#include "llm/provider.hpp"

namespace swiftagent {

class OpenAIProvider final : public Provider {
public:
    explicit OpenAIProvider(std::string base_url, std::string api_key,
                            std::string model = "gpt-4o-mini",
                            std::string path = "/v1/chat/completions");

    [[nodiscard]] Result<ModelResponse> complete(const Messages& context) override;
    [[nodiscard]] std::string name() const override { return "openai"; }

private:
    std::string base_url_;
    std::string api_key_;
    std::string model_;
    std::string path_;
    httplib::Client client_;
};

} // namespace swiftagent
