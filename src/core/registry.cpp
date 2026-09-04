#include "core/registry.hpp"

#include <algorithm>

namespace swiftagent {

void ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
    if (!tool) {
        return;
    }
    std::lock_guard<std::mutex> lock(mtx_);
    auto name = tool->descriptor().name;
    tools_[std::move(name)] = std::move(tool);
}

Tool* ToolRegistry::find(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::vector<ToolDescriptor> ToolRegistry::list() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<ToolDescriptor> out;
    out.reserve(tools_.size());
    for (const auto& [_, tool] : tools_) {
        out.push_back(tool->descriptor());
    }
    std::sort(out.begin(), out.end(),
              [](const ToolDescriptor& a, const ToolDescriptor& b) { return a.name < b.name; });
    return out;
}

std::vector<std::string> ToolRegistry::names() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> out;
    out.reserve(tools_.size());
    for (const auto& [name, _] : tools_) {
        out.push_back(name);
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace swiftagent
