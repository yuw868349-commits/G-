# 路线图

记一下接下来要做的事，方便外部贡献者对齐方向。

## 短期（v0.2）

- 真实 LLM 联调：用真 OpenAI 兼容端点跑通整条链路，录一段端到端示例
- Windows 平台层 CI 真跑通：已在 windows-latest 矩阵里 build+ctest，平台抽象已对齐
- MCP SSE transport 接一个真 SSE 服务端做联调
- 把 codecov 接上 CI，README 加覆盖率徽章
- 单元测试覆盖到 80% 以上

## 中期（v0.3 - v0.5）

- 分布式 worker：把编排器拆成可水平扩展的执行节点
- 流式输出：模型 token 级别回调，前端能看到打字机效果
- 工具沙箱：把工具调用隔离在受限进程里
- 完整可观测性：OpenTelemetry 协议导出 trace
- 嵌入式模式：可以 `dlopen` 进别的进程当库用

## 长期（v1.0）

- API 稳定到 1.0：破坏性变更需要走 deprecation 流程
- 文档站：doxygen 之外补用户指南和教程
- 多语言 SDK：除了 Python 加 Node.js 和 Go
- 端到端加密：MCP 走 TLS，敏感任务用本地模型
- 性能基准：把 bench 数据定期发布，做回归对比

## 设计原则

- 简单胜于聪明：一个组件做一件事，组合用接口
- 失败要明显：错误码、错误信息、stack trace 都要给齐
- 性能可观察：遥测是核心，不是装饰
- 跨平台一致：Linux / macOS / Windows 行为要一致
- 后向兼容：API 改动要有迁移路径，不能说断就断
