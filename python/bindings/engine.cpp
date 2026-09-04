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

std::unique_ptr<praxis::Provider> make_provider(const std::string& name,
                                                    const std::string& model,
                                                    const std::string& api_key,
                                                    const std::string& api_base) {
    if (name == "openai") {
        std::string base = api_base.empty() ? "https://api.openai.com/v1" : api_base;
        return std::make_unique<praxis::OpenAIProvider>(base, api_key, model, "/chat/completions");
    }
    auto fake = std::make_unique<praxis::FakeProvider>();
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
          orchestrator_(new praxis::Orchestrator(*provider_)),
          budget_turns_(budget_turns) {
        orchestrator_->register_builtin();
    }

    praxis::RunResult run(const std::string& task, std::uint32_t max_turns) {
        praxis::Budget budget;
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

    praxis::Telemetry& telemetry() { return orchestrator_->telemetry(); }
    praxis::Replay& replay() { return orchestrator_->replay(); }
    praxis::ToolRegistry& registry() { return orchestrator_->registry(); }

private:
    std::unique_ptr<praxis::Provider> provider_;
    std::unique_ptr<praxis::Orchestrator> orchestrator_;
    std::uint32_t budget_turns_;
};

} // namespace

// GIL policy: every function whose body calls into the engine releases the
// GIL before the call and reacquires it on return. Tool handlers in Python
// run with the GIL held; the executor acquires it around handler dispatch.

PYBIND11_MODULE(praxis_native, m) {
    m.doc() = "Praxis native bindings";

    py::class_<praxis::RunResult>(m, "RunResult")
        .def_readonly("completed", &praxis::RunResult::completed)
        .def_readonly("bounded", &praxis::RunResult::bounded)
        .def_readonly("turns", &praxis::RunResult::turns)
        .def_readonly("final_output", &praxis::RunResult::final_output);

    py::class_<praxis::Telemetry>(m, "Telemetry")
        .def("report", [](const praxis::Telemetry& t) { return t.report().dump(); })
        .def_property_readonly("speedup_x", &praxis::Telemetry::speedup_x)
        .def_property_readonly("prompt_tokens", &praxis::Telemetry::prompt_tokens)
        .def_property_readonly("completion_tokens", &praxis::Telemetry::completion_tokens)
        .def_property_readonly("cache_hits", &praxis::Telemetry::cache_hits)
        .def_property_readonly("cache_misses", &praxis::Telemetry::cache_misses);

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
