# Contributing

## 提 PR

1. fork 仓库，建分支（`feat/xxx`、`fix/xxx`）
2. 写代码、测试
3. 本地跑过：
   ```bash
   cmake --build build -j
   ctest --test-dir build --output-on-failure
   ```
4. 提 PR，模板会自动加载

## 提交信息

用 conventional commits：`feat:`、`fix:`、`docs:`、`refactor:`、`test:`、`chore:`。

## 改动约定

- C++ 跟着现有风格：snake_case 变量、PascalCase 类型、kPascalCase 常量
- 新公开 API 在头文件加注释
- 加新功能必须有单元测试
- 改 CLI 输出或 SDK 行为，更新 README

## 行为准则

尊重他人，对事不对人。
