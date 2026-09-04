#include "ui/web.hpp"

#include <atomic>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "core/orchestrator.hpp"
#include "core/telemetry.hpp"

namespace praxis {

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

// Decode a single base64 character to its 6-bit value.  Returns
// 0xff for an invalid character.
unsigned char b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<unsigned char>(c - 'A');
    if (c >= 'a' && c <= 'z') return static_cast<unsigned char>(26 + c - 'a');
    if (c >= '0' && c <= '9') return static_cast<unsigned char>(52 + c - '0');
    if (c == '+') return 62;
    if (c == '/') return 63;
    return 0xff;
}

// Decode a base64 string.  Used for HTTP basic-auth credentials.
std::string b64_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3 / 4);
    int bits = 0;
    unsigned int buf = 0;
    for (char c : in) {
        if (c == '=' || c == '\r' || c == '\n' || c == ' ') continue;
        auto v = b64_value(c);
        if (v == 0xff) return {};
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xff));
        }
    }
    return out;
}

std::string index_html() {
    return R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Praxis Panel</title>
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
<header><h1 style="margin:0">Praxis Panel</h1></header>
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
    // The shell tool can execute arbitrary programs on the host, so
    // the web panel must not be a public service by default.  Default
    // to loopback; operators that intentionally want to expose the
    // panel to the network must pass --web-host explicitly.
    const std::string& host = opts.web_host;
    const bool require_auth = !opts.web_user.empty() && !opts.web_pass.empty();

    httplib::Server server;

    // Reject requests that don't carry valid basic-auth credentials
    // when one has been configured.  This is *additional* defence on
    // top of binding to loopback; a misconfigured operator who
    // exposes the panel publicly still needs to set credentials.
    auto check_auth = [require_auth, &opts](const httplib::Request& req) -> bool {
        if (!require_auth) return true;
        auto it = req.headers.find("Authorization");
        if (it == req.headers.end()) return false;
        const std::string& value = it->second;
        const std::string prefix = "Basic ";
        if (value.size() <= prefix.size() ||
            value.compare(0, prefix.size(), prefix) != 0) {
            return false;
        }
        auto decoded = b64_decode(value.substr(prefix.size()));
        auto colon = decoded.find(':');
        if (colon == std::string::npos) return false;
        auto user = decoded.substr(0, colon);
        auto pass = decoded.substr(colon + 1);
        return user == opts.web_user && pass == opts.web_pass;
    };

    server.Get("/", [check_auth](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req)) {
            res.status = 401;
            res.set_header("WWW-Authenticate", "Basic realm=\"praxis\"");
            res.set_content("authentication required", "text/plain");
            return;
        }
        res.set_content(index_html(), "text/html");
    });
    server.Post("/run", [opts, check_auth](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req)) {
            res.status = 401;
            res.set_header("WWW-Authenticate", "Basic realm=\"praxis\"");
            res.set_content("authentication required", "text/plain");
            return;
        }
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
    if (host == "0.0.0.0" || host.empty() || host == "::") {
        std::cerr << "warning: web panel bound to '" << host
                  << "' exposes the shell tool to the network. "
                  << "Pass --web-user/--web-pass or bind to 127.0.0.1.\n";
    }
    std::cout << "praxis web panel listening on http://" << host
              << ":" << opts.web_port
              << (require_auth ? " (basic-auth required)" : "")
              << "\n";
    return server.listen(host, opts.web_port) ? 0 : 1;
}

} // namespace praxis
