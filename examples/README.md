# Examples

`cmake --build build` 会在 `build/` 目录生成下面这些可执行文件。每个都独立可跑。

## 列表

| 文件 | 说明 |
| --- | --- |
| `01_minimal.cpp` | 用 FakeProvider 跑最简任务 |
| `02_custom_tool.cpp` | 注册一个自定义 Tool，跑带工具调用的任务 |
| `03_telemetry.cpp` | 看遥测：tokens、cache、模块耗时、speedup |
| `04_cache.cpp` | 直接用依赖感知的 Cache |
| `05_mcp_stdio.cpp` | 通过 stdio 把外部 MCP 服务接进 ToolRegistry |

## 跑

```bash
cmake -S . -B build -DSWIFTAGENT_BUILD_EXAMPLES=ON
cmake --build build --target example_minimal example_custom_tool \
                              example_telemetry example_cache example_mcp

./build/example_minimal
./build/example_custom_tool
./build/example_telemetry
./build/example_cache
python3 examples/scripts/echo_mcp.py &  # MCP 例子需要这个
./build/example_mcp
```

## 写新例子

在 `examples/cpp/` 加一个 `NN_xxx.cpp`，CMakeLists 里同步加一行 `add_executable` 就行。每个例子尽量自包含，依赖少，复用 FakeProvider 演示 API 行为。
