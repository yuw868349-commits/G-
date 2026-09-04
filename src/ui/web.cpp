#include "ui/web.hpp"

#include <atomic>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#include "core/orchestrator.hpp"
#include "core/telemetry.hpp"

namespace swiftagent {

namespace {

class ConsoleSink final : public EventSink {
public:
    void on_event(const Event& event) override {
        std::lock_guard<std::mutex> lock(mtx_);
        stream << "{\"seq\":" << event.sequence
               << ",\"kind\":" << static_cast<int>(event.kind)
               << ",\"payload\":" << event.payload.dump() << "}\n";
    }
    std::stringstream stream;
    std::mutex mtx_;
};

std::string index_html() {
    return R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>SwiftAgent Panel</title>
<style>
  body { font-family: -apple-system, system-ui, sans-serif; margin: 0; background: #111; color: #eee; }
  header { padding: 16px 24px; background: #1f1f1f; border-bottom: 1px solid #333; }
  main { padding: 16px 24px; }
  textarea { width: 100%; height: 80px; background: #1a1a1a; color: #eee; border: 1px solid #333; padding: 8px; }
  button { padding: 8px 16px; background: #2d6cdf; color: #fff; border: 0; cursor: pointer; }
  pre { background: #1a1a1a; padding: 12px; max-height: 50vh; overflow: auto; border: 1px solid #333; }
  .row { display: flex; gap: 8px; margin-bottom: 12px; align-items: center; }
  label { color: #aaa; font-size: 12px; }
  input { background: #1a1a1a; color: #eee; border: 1px solid #333; padding: 6px; }
</style>
</head>
<body>
<header><h1 style="margin:0">SwiftAgent Panel</h1></header>
<main>
  <div class="row">
    <label>Task</label>
    <textarea id="task">summarize the project</textarea>
  </div>
  <div class="row">
    <label>Provider</label>
    <input id="provider" value="fake" />
    <label>Budget</label>
    <input id="budget" type="number" value="8" />
    <button onclick="runTask()">Run</button>
  </div>
  <h3>Output</h3>
  <pre id="output">(idle)</pre>
  <h3>Telemetry</h3>
  <pre id="telemetry">(none yet)</pre>
  <h3>Events</h3>
  <pre id="events"></pre>
</main>
<script>
  async function runTask() {
    const body = {
      task: document.getElementById('task').value,
      provider: document.getElementById('provider').value,
      budget_turns: parseInt(document.getElementById('budget').value, 10)
    };
    document.getElementById('output').textContent = 'running...';
    try {
      const res = await fetch('/run', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body) });
      const json = await res.json();
      document.getElementById('output').textContent = json.final_output || JSON.stringify(json, null, 2);
      document.getElementById('telemetry').textContent = JSON.stringify(json.telemetry || {}, null, 2);
      document.getElementById('events').textContent = (json.events || []).join('\n');
    } catch (err) {
      document.getElementById('output').textContent = 'error: ' + err.message;
    }
  }
</script>
</body>
</html>
)HTML";
}

} // namespace

int run_web(const CliOptions& opts) {
    httplib::Server server;
    server.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(index_html(), "text/html");
    });
    server.Post("/run", [opts](const httplib::Request& req, httplib::Response& res) {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.is_object()) {
            res.status = 400;
            res.set_content("invalid json", "text/plain");
            return;
        }
        CliOptions local = opts;
        local.task = body.value("task", std::string{});
        local.provider_name = body.value("provider", std::string{"fake"});
        local.budget_turns = body.value("budget_turns", 8u);
        if (local.task.empty()) {
            res.status = 400;
            res.set_content("missing task", "text/plain");
            return;
        }
        auto provider = make_provider_from_cli(local);
        Orchestrator orch(*provider);
        orch.register_builtin();
        auto sink = std::make_shared<ConsoleSink>();
        orch.attach_observer(sink);
        Budget budget;
        budget.max_turns = local.budget_turns;
        budget.active = true;
        auto result = orch.run(local.task, budget);
        nlohmann::json out;
        if (result.ok()) {
            out["final_output"] = result.value().final_output;
            out["turns"] = result.value().turns;
            out["completed"] = result.value().completed;
        } else {
            out["error"] = result.error().message;
        }
        out["telemetry"] = orch.telemetry().report();
        out["events"] = sink->stream.str();
        res.set_content(out.dump(), "application/json");
    });
    std::cout << "swiftagent web panel listening on http://0.0.0.0:" << opts.web_port << "\n";
    return server.listen("0.0.0.0", opts.web_port) ? 0 : 1;
}

} // namespace swiftagent
