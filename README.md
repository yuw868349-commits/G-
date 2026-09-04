# SwiftAgent

C++23 写的 agent 执行引擎。能跑 plan-act-reflect 的多轮任务，自带上下文管理、工具调用、缓存、回放、遥测。MCP 协议把远程工具接进来，Python SDK 也能调。

## 仓库内容

- C++23 核心：编排器、事实存储、上下文管理、工具执行器、缓存、回放、遥测、模型级联
- MCP host：stdio 和 SSE 两种传输，本地或远程工具都能挂
- 平台层：POSIX（Linux/macOS）和 Windows 两套后端
- CLI + Web 面板
- Python SDK（pybind11 封装 C++ 核心）
- Benchmark 框架，三个标准负载
- 单元测试 48 个，集成测试 + Python smoke test

## 架构

```
                +-------------------+
                |    Python SDK     |
                |   (pybind11)      |
                +---------+---------+
                          |
+-------------------------v--------------------------+
|                   C++ Core                        |
|                                                    |
|  Orchestrator  ->  Context Manager  ->  Model      |
|       |                  |                Cascade  |
|       v                  v                         |
|  Tool Executor      Fact Store / Digest            |
|       |                  |                         |
|       v                  v                         |
|  MCP Host (stdio/SSE)   Replay / Telemetry         |
+-------------------------+--------------------------+
                          |
                          v
                +-------------------+
                |   Platform Layer  |
                |  POSIX / Windows  |
                +-------------------+
```

数据流：用户给个任务，编排器拉模型输出，调工具，工具结果写进事实存储，上下文管理器把历史压缩到当前窗口，遥测收集耗时。回放器记录所有事件，重跑可复现。

## 构建

需要：
- CMake 3.24+
- C++23 编译器（GCC 13+ / Clang 17+ / MSVC 19.30+）
- Python 3.10+（跑 Python SDK）
- nlohmann_json、httplib、catch2 自动下载

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Python SDK

```bash
cmake -S python -B build/python -DCMAKE_PREFIX_PATH=$(python -c "import pybind11;print(pybind11.get_cmake_dir())")
cmake --build build/python -j
PYTHONPATH=python python -c "import swiftagent; print(swiftagent.Engine)"
```

简单跑一下：

```python
import swiftagent

engine = swiftagent.Engine(provider="fake", budget_turns=5)
result = engine.run("把 in 目录的文件按扩展名分到 out/")
print(result.turns, result.completed)
print(engine.telemetry().report())
```

## CLI

```bash
./build/swiftagent run --provider fake --budget 10 "把 in 目录的文件按扩展名分到 out/"
```

## Web 面板

```bash
./build/swiftagent web --port 8080
```

打开浏览器看任务进度和遥测数据。

## Benchmark

```bash
./build/bench_main
```

跑三个标准负载（文件重整、数据收集、依赖安装），对比串行和并行的加速比。

## 测试

```bash
# C++
ctest --test-dir build

# Python
PYTHONPATH=python python -m pytest tests/python -v
```

## 协议

MCP host 支持：
- stdio：起一个子进程跑 MCP 服务
- SSE：通过 HTTP + Server-Sent Events 调远程服务

工具注册后用统一接口调用，编排器不关心是本地还是远程。

## 贡献

PR 走 `.github/pull_request_template.md` 那个流程。提交前跑测试。改动大先开 issue 讨论。

## License

MIT，详见 [LICENSE](LICENSE)。
