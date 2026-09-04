#pragma once

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace swiftagent {

using HardKeys = std::map<std::string, std::string>;

class Normalizer {
public:
    [[nodiscard]] static HardKeys hard_keys(const std::string& content);
    [[nodiscard]] static bool contains_key(const HardKeys& keys, const std::string& key);
    [[nodiscard]] static std::string value_of(const HardKeys& keys, const std::string& key);
};

struct SoftFingerprint {
    std::vector<double> embedding;
    double confidence{0.0};

    [[nodiscard]] bool is_high_confidence(double threshold) const noexcept {
        return confidence >= threshold;
    }
};

std::size_t estimate_tokens(const std::string& text);

class Digest {
public:
    Digest() = default;

    [[nodiscard]] static Digest build(const std::string& type, const std::string& content);

    [[nodiscard]] const HardKeys& hard_keys() const noexcept { return hard_keys_; }
    [[nodiscard]] std::string hard_fingerprint() const noexcept { return hard_fingerprint_; }
    [[nodiscard]] const SoftFingerprint& soft() const noexcept { return soft_; }
    [[nodiscard]] bool matches_hard(const Digest& other) const noexcept {
        return hard_fingerprint_ == other.hard_fingerprint_;
    }

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static Digest from_json(const nlohmann::json& json);

    [[nodiscard]] bool is_empty() const noexcept { return hard_fingerprint_.empty(); }

private:
    HardKeys hard_keys_;
    std::string hard_fingerprint_;
    SoftFingerprint soft_;
};

} // namespace swiftagent
