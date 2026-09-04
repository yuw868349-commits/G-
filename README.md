# SwiftAgent

> **Name disambiguation.** SwiftAgent is a **C++23 agent execution
> engine**. It is *not* related to the Apple/Swift programming language,
> nor to any other project that happens to share the name. The "Swift"
> in SwiftAgent refers to the engine's goal of being a *swift* (fast /
> responsive) agent runtime. If you arrived here looking for the Swift
> language server, SwiftUI, or Apple's open-source Swift packages,
> please visit [swift.org](https://swift.org) instead.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C.svg?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.24%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![Python 3.10+](https://img.shields.io/badge/Python-3.10%2B-3776AB?logo=python&logoColor=white)](https://www.python.org/)
[![Release v0.1.0](https://img.shields.io/badge/release-v0.1.0-blue.svg)](https://github.com/yuw868349-commits/swift-agent/releases/tag/v0.1.0)
[![CI](https://img.shields.io/github/actions/workflow/status/yuw868349-commits/swift-agent/ci.yml?branch=main&label=CI&logo=github)](https://github.com/yuw868349-commits/swift-agent/actions/workflows/ci.yml)
[![codecov](https://img.shields.io/codecov/c/github/yuw868349-commits/swift-agent?logo=codecov)](https://codecov.io/gh/yuw868349-commits/swift-agent)
[![Wheels](https://img.shields.io/github/actions/workflow/status/yuw868349-commits/swift-agent/wheel.yml?label=wheels&logo=github)](https://github.com/yuw868349-commits/swift-agent/actions/workflows/wheel.yml)
[![Platforms](https://img.shields.io/badge/platforms-linux%20%7C%20macOS%20%7C%20windows-lightgrey?logo=linux&logoColor=white)](#构建)
[![MCP](https://img.shields.io/badge/MCP-stdio%20%7C%20SSE-6f42c1)](#协议)
[![Docs](https://img.shields.io/badge/docs-doxygen-blueviolet)](https://github.com/yuw868349-commits/swift-agent/actions/workflows/docs-pages.yml)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.md)
[![Issues](https://img.shields.io/github/issues/yuw868349-commits/swift-agent)](https://github.com/yuw868349-commits/swift-agent/issues)
[![Last Commit](https://img.shields.io/github/last-commit/yuw868349-commits/swift-agent)](https://github.com/yuw868349-commits/swift-agent/commits/main)
[![Code of Conduct](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](CODE_OF_CONDUCT.md)

C++23 写的 agent 执行引擎。能跑 plan-act-reflect 的多轮任务，自带上下文管理、工具调用、缓存、回放、遥测。MCP 协议把远程工具接进来，Python SDK 也能调。

## 仓库内容

- C++23 核心：编排器、事实存储、上下文管理、工具执行器、缓存、回放、遥测、模型级联
- MCP host：stdio 和 SSE 两种传输，本地或远程工具都能挂
- 平台层：POSIX（Linux/macOS）和 Windows 两套后端
- CLI + Web 面板
- Python SDK（pybind11 封装 C++ 核心）
- Benchmark 框架，三个标准负载
- 单元测试 48 个，集成测试 + Python smoke test
- 5 个可跑示例（`examples/cpp/`）
- Doxygen 文档（`Doxyfile`）
- CI：三平台编译 + ctest + Python + ASan + UBSan + TSan + 覆盖率 + 格式检查
- Docker 多阶段镜像（CLI / wheel / dev）
- 包配方：deb / rpm / homebrew / winget

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

## 示例

`examples/cpp/` 下有 5 个独立可跑的例子，覆盖最小任务、自定义工具、遥测、缓存、MCP stdio 接入。详见 [examples/README.md](examples/README.md)。

```bash
cmake --build build --target example_minimal example_custom_tool \
                              example_telemetry example_cache example_mcp
./build/example_minimal
```

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

## 文档

API 参考用 Doxygen 生成：

```bash
doxygen Doxyfile
open docs/doxygen/html/index.html
```

CI 每次提交都跑一遍，把 HTML 当 artifact 上传。

## Docker

```bash
docker build -t swiftagent:dev -f docker/Dockerfile.cli .
docker run --rm swiftagent:dev --help
```

开发镜像（带工具链、Python、例子、测试）：

```bash
docker build -t swiftagent:dev-full -f docker/Dockerfile.dev .
docker run --rm -it swiftagent:dev-full bash
```

Python 打包：

```bash
docker build -t swiftagent:wheel -f docker/Dockerfile.wheel .
```

## 包管理器

配方放在 `packaging/`：
- `deb/build.sh`：用 cmake + dpkg-deb 生成 `.deb`
- `rpm/swiftagent.spec`：Fedora / RHEL spec
- `homebrew/swiftagent.rb`：macOS Homebrew formula
- `winget/swiftagent.yaml`：Windows 包管理器清单

## 引用

学术工作用的话用 [CITATION.cff](CITATION.cff) 里的元数据，GitHub 会显示 "Cite this repository" 按钮。

## 贡献

PR 走 `.github/pull_request_template.md` 那个流程。提交前跑测试。改动大先开 issue 讨论。

行为准则见 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。

## License

MIT，详见 [LICENSE](LICENSE)。
