# File health CI 的 prek 检查范围修复

日期：2026-11-13。

## 故障原因

`CI - File health` 使用 `j178/prek-action@v2`，默认执行 `prek run --all-files`。日志中的 prek 0.5.10 会自动发现子目录中的配置，因此同时加载了：

- 根目录 `.pre-commit-config.yaml`。
- `deps/libtorrent/2.0.15/.pre-commit-config.yaml`。
- `deps/libtorrent/2.1.2/.pre-commit-config.yaml`。

根配置已有 `exclude: ^deps/libtorrent/`，但这个选项只过滤根项目传给 hook 的文件，不会禁用子项目。日志显示两版 libtorrent 的 Python 检查和代码生成任务仍被执行，最后 2.1.2 的格式化 hook 因 runner 没有 `git-clang-format-18` 而中止：

```text
error: Failed to run hook `git-clang-format`
  caused by: Failed to run `git-clang-format-18 --binary clang-format-18 --force`
  caused by: No such file or directory (os error 2)
```

这是检查范围问题。该 CI 原有配置明确将内置 libtorrent 排除在 qBittorrent 文件检查之外。

## 修改内容

文件：`.github/workflows/ci_file_health.yaml`，`Check files` 步骤。

给 Action 增加参数：

```yaml
with:
  extra-args: --all-files --config .pre-commit-config.yaml
```

prek 的 `--config` 会禁用自动发现，只加载指定配置。保留 `--all-files`，继续运行 qBittorrent 根项目的全部文件检查；根配置中的依赖排除规则继续生效。

此修改同时覆盖 **libtorrent 2.0.15 和 2.1.2**，保留依赖目录中的上游配置和所有应用源码。两版 libtorrent 的构建及功能测试继续由各平台 CI 执行。

参考：

- [prek 0.5.10 工作区文档](https://github.com/j178/prek/blob/v0.5.10/docs/reference/workspace.md)，`Files and working directories` 和 `Single config mode`。
- [prek Action 参数定义](https://github.com/j178/prek-action/blob/v2/action.yml)，`extra-args` 会附加到 `prek run`。

## 验证

- 工作流 YAML 解析通过，确认 Action 使用 `--all-files --config .pre-commit-config.yaml`。
- 临时下载与 CI 一致的 prek **0.5.10**，以指定根配置列出 hook，只包含 qBittorrent 的 **10 个检查**，没有 libtorrent 的 `git-clang-format`、Python 类型检查或代码生成任务。
- 使用真实根配置执行 `--all-files --dry-run --verbose` 成功，确认检查候选文件不含 `deps/libtorrent/`，且 qBittorrent 的 YAML、翻译、界面和源码文件仍进入相应检查。
- 对本次修改的工作流和记录文件执行实际根项目 hook 检查。
- `git diff --check` 通过。

本机是 macOS，验证针对与 CI 一致的 prek 版本、真实配置和检查范围。Ubuntu 的整条 File health 工作流仍需重新运行。临时工具和缓存创建在 `/private/tmp`，验证后清理。
