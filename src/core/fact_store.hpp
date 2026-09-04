#pragma once

#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

namespace praxis {

struct Fact {
    std::string id;
    std::string type;
    std::string content;
};

class FactStore {
public:
    explicit FactStore(std::string path = "");
    ~FactStore();

    FactStore(const FactStore&) = delete;
    FactStore& operator=(const FactStore&) = delete;
    FactStore(FactStore&&) = delete;
    FactStore& operator=(FactStore&&) = delete;

    [[nodiscard]] std::string append(const std::string& type, const std::string& content);
    [[nodiscard]] std::optional<Fact> get(const std::string& id) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    void flush();

private:
    [[nodiscard]] static std::string digest_hex(const std::string& type,
                                                const std::string& content);

    std::string path_;
    std::unordered_map<std::string, Fact> entries_;
    std::ofstream out_;
};

} // namespace praxis
