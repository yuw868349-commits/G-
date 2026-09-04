# Contributing

## 行为准则

所有参与者都要遵守 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。

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

`release-please` 会自动根据这些前缀决定版本号和 CHANGELOG：
- `feat:` → minor version bump
- `fix:` → patch version bump
- 标记 `!` 或 `BREAKING CHANGE:` → major version bump

## 改动约定

- C++ 跟着现有风格：snake_case 变量、PascalCase 类型、kPascalCase 常量
- 新公开 API 在头文件加注释
- 加新功能必须有单元测试
- 改 CLI 输出或 SDK 行为，更新 README
- 改完跑 `clang-format -i` 整自己的改动

## 签名 commit（可选）

如果仓库开了 "Require signed commits"，本地需要配 GPG 或 SSH 签名（详见 [SECURITY.md](SECURITY.md)）。

## 行为准则

尊重他人，对事不对人。
