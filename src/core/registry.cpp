#include "core/registry.hpp"
#include "tools/tool.hpp"

#include <algorithm>

namespace swiftagent {

namespace {

// Default implementation of `Tool::resources_for`: returns the
// tool's statically-declared resources, which makes any two calls
// of the same tool conflict in the dependency graph.  Tools that
// touch concrete addresses (file paths, URLs) should override this.
std::vector<std::string> default_resources_for(const Tool& tool,
                                               const ToolCall& /*call*/) {
    return tool.descriptor().declared_resources;
}

}  // namespace

// Out-of-line definition of the virtual default so concrete tools
// don't have to ship one.  The orchestrator pulls it in via
// `Tool::resources_for` (virtual dispatch).
std::vector<std::string> Tool::resources_for(const ToolCall& call) const {
    return default_resources_for(*this, call);
}

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
