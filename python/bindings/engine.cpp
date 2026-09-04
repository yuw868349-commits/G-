#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>

#include "core/orchestrator.hpp"
#include "llm/fake_provider.hpp"
#include "llm/openai_provider.hpp"
#include "tools/registry.hpp"
#include "tools/builtin.hpp"

namespace py = pybind11;

namespace {

std::unique_ptr<swiftagent::Provider> make_provider(const std::string& name,
                                                    const std::string& model,
                                                    const std::string& api_key,
                                                    const std::string& api_base) {
    if (name == "openai") {
        std::string base = api_base.empty() ? "https://api.openai.com/v1" : api_base;
        return std::make_unique<swiftagent::OpenAIProvider>(base, api_key, model, "/chat/completions");
    }
    auto fake = std::make_unique<swiftagent::FakeProvider>();
    fake->script({
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    return fake;
}

class Engine {
public:
    Engine(const std::string& provider, const std::string& model,
           const std::string& api_key, const std::string& api_base,
           std::uint32_t budget_turns)
        : provider_(make_provider(provider, model, api_key, api_base)),
          orchestrator_(new swiftagent::Orchestrator(*provider_)),
          budget_turns_(budget_turns) {
        orchestrator_->register_builtin();
    }

    swiftagent::RunResult run(const std::string& task, std::uint32_t max_turns) {
        swiftagent::Budget budget;
        budget.max_turns = max_turns > 0 ? max_turns : budget_turns_;
        budget.active = true;
        py::gil_scoped_release release;
        auto result = orchestrator_->run(task, budget);
        py::gil_scoped_acquire acquire;
        if (!result.ok()) {
            throw std::runtime_error(result.error().message);
        }
        return result.value();
    }

    swiftagent::Telemetry& telemetry() { return orchestrator_->telemetry(); }
    swiftagent::Replay& replay() { return orchestrator_->replay(); }
    swiftagent::ToolRegistry& registry() { return orchestrator_->registry(); }

private:
    std::unique_ptr<swiftagent::Provider> provider_;
    std::unique_ptr<swiftagent::Orchestrator> orchestrator_;
    std::uint32_t budget_turns_;
};

} // namespace

// GIL policy: every function whose body calls into the engine releases the
// GIL before the call and reacquires it on return. Tool handlers in Python
// run with the GIL held; the executor acquires it around handler dispatch.

PYBIND11_MODULE(swiftagent_native, m) {
    m.doc() = "SwiftAgent native bindings";

    py::class_<swiftagent::RunResult>(m, "RunResult")
        .def_readonly("completed", &swiftagent::RunResult::completed)
        .def_readonly("bounded", &swiftagent::RunResult::bounded)
        .def_readonly("turns", &swiftagent::RunResult::turns)
        .def_readonly("final_output", &swiftagent::RunResult::final_output);

    py::class_<swiftagent::Telemetry>(m, "Telemetry")
        .def("report", [](const swiftagent::Telemetry& t) { return t.report().dump(); })
        .def_property_readonly("speedup_x", &swiftagent::Telemetry::speedup_x)
        .def_property_readonly("prompt_tokens", &swiftagent::Telemetry::prompt_tokens)
        .def_property_readonly("completion_tokens", &swiftagent::Telemetry::completion_tokens)
        .def_property_readonly("cache_hits", &swiftagent::Telemetry::cache_hits)
        .def_property_readonly("cache_misses", &swiftagent::Telemetry::cache_misses);

    py::class_<Engine>(m, "Engine")
        .def(py::init<const std::string&, const std::string&, const std::string&,
                      const std::string&, std::uint32_t>(),
             py::arg("provider") = "fake", py::arg("model") = "gpt-4o-mini",
             py::arg("api_key") = "", py::arg("api_base") = "",
             py::arg("budget_turns") = 32)
        .def("run", &Engine::run, py::arg("task"), py::arg("max_turns") = 0)
        .def("telemetry", &Engine::telemetry, py::return_value_policy::reference_internal)
        .def("replay", &Engine::replay, py::return_value_policy::reference_internal)
        .def("registry", &Engine::registry, py::return_value_policy::reference_internal);
}
