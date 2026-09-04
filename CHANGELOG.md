# 更新记录

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

## [0.2.0](https://github.com/yuw868349-commits/praxis/compare/v0.1.0...v0.2.0) (2026-09-04)


### Features

* add clang-format/tidy, doxygen, sanitizer+coverage CI, and example programs ([0b8ed82](https://github.com/yuw868349-commits/praxis/commit/0b8ed82681a69a8951733d4b3d1a8aa157ea9a7a))
* add codecov integration, github pages docs, release-please, code of conduct ([f2e8d51](https://github.com/yuw868349-commits/praxis/commit/f2e8d51b3073f73c85d74bbfed4a1d48b610bb96))
* add dockerfiles, citation, roadmap, package recipes, and label bootstrap script ([591bcbe](https://github.com/yuw868349-commits/praxis/commit/591bcbe485ef92c9aef9d830b3a38ab2082ecd2a))
* **digest:** real bag-of-ngrams soft embedding for approximate recall ([a523225](https://github.com/yuw868349-commits/praxis/commit/a52322535f1a879ffa2ee6ecec9a9ffd5868b343))
* **llm:** add RetryingProvider with exponential backoff ([59214c4](https://github.com/yuw868349-commits/praxis/commit/59214c4d938df44d3353c5ff3dc5444f06008a67))
* **router:** wire ProviderRouter into the orchestrator cascade ([9bedc32](https://github.com/yuw868349-commits/praxis/commit/9bedc326b75a5320b598cdc0ef26d92b0f1cc9b1))
* 你好 ([14f4e3b](https://github.com/yuw868349-commits/praxis/commit/14f4e3b5728edefa7965c183e267b4bb3ef0969b))


### Bug Fixes

* **fact_store:** escape control characters in on-disk format ([20a1353](https://github.com/yuw868349-commits/praxis/commit/20a135315f211d830622728d4a59a5920f1948ac))
* **mcp:** correct tools/call method, real SSE transport, fix transport leak ([2270981](https://github.com/yuw868349-commits/praxis/commit/2270981c8342d1396d1872618ab9dbae0cddb767))
* **orchestrator:** actually route through RetryingProvider and drop dead code ([7177fcc](https://github.com/yuw868349-commits/praxis/commit/7177fcc76fb4ee47e895c86d6a52768cb7aaee48))
* **orchestrator:** parse OpenAI-style string tool arguments correctly ([25af05e](https://github.com/yuw868349-commits/praxis/commit/25af05e6d66fbf9d98233e404703634a59fded11))
* **tools:** builtin read_file/shell use real ToolContext, fix JSON schemas ([a27c2ba](https://github.com/yuw868349-commits/praxis/commit/a27c2ba707b70730451ab100fff95893bc1a7feb))

## [0.1.0] - 2026-09-04

### 加了
- 核心引擎：Orchestrator、Context Manager、Fact Store、Digest、Tool Executor、Cache、Replay、Telemetry、Model Cascade
- MCP host：stdio 和 SSE 两种传输
- 平台层：POSIX 和 Windows 后端
- CLI 和 Web 面板
- Python SDK（pybind11）
- Benchmark 框架，三个标准负载
- 单元测试 48 个，集成测试，Python smoke test
- CI workflow（wheel 构建）

### 改的
- 无

### 修的
- 无

### 安全
- 无
