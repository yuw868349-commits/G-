#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "tools/tool.hpp"

namespace swiftagent {

class ToolRegistry {
public:
    void register_tool(std::unique_ptr<Tool> tool);
    [[nodiscard]] Tool* find(const std::string& name) const;
    [[nodiscard]] std::vector<ToolDescriptor> list() const;
    [[nodiscard]] std::vector<std::string> names() const;

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
};

} // namespace swiftagent
