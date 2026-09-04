# 安全问题

发现安全漏洞别在 issue 里公开提，发邮件给仓库 owner 或者开一个 private security advisory。

报告里写清楚：
- 影响范围
- 复现步骤
- 触发条件

我们会在 7 天内回。

## 启用 commit 签名验证

仓库可以要求所有 commit 必须 GPG/SSH 签名。在 GitHub 网页上 Settings > Branches > Branch protection rules > main 找到 "Require signed commits" 打勾即可。

本地配 GPG 签名：

```bash
gpg --full-generate-key
gpg --list-secret-keys --keyid-format=long
git config --global user.signingkey <KEY_ID>
git config --global commit.gpgsign true
```

SSH 签名更简单：

```bash
ssh-keygen -t ed25519 -C "you@example.com" -f ~/.ssh/gh-sign
gh ssh-agent --config  # 一次
git config --global gpg.format ssh
git config --global user.signingkey ~/.ssh/gh-sign.pub
git config --global commit.gpgsign true
```

签名后把公钥加到 GitHub Settings > SSH and GPG keys。

> 注：自动化 agent 提的 commit 默认不签名。如果要严格签名验证，先在本地配好签名再让 agent 跑。
