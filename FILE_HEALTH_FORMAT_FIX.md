# File health CI 的翻译和文档格式修复

日期：2026-11-13。

## 故障原因

指定根配置后，prek 正常执行 qBittorrent 的文件检查。本次日志有三个 hook 修改文件，因此 CI 返回 1：

1. `check-webui-translation-tag` 重新序列化 `webui_en.ts` 和 `webui_zh_CN.ts`。
2. `end-of-file-fixer` 修正 `UPLOAD_RATIO_MIGRATION.md` 的末尾空行。
3. `rumdl` 修正 `PYTHON_CI_SCOPE_FIX.md` 和 `UPLOAD_RATIO_MIGRATION.md` 中六个表格的分隔行空格。

检查器已给出可自动修正的问题。将修正保存到仓库后，CI 才能在复跑时通过。

## 修改内容

| 文件 | 修改 |
| --- | --- |
| `src/webui/www/translations/webui_en.ts` | 用现有 WebUI 翻译 hook 规范化序列化输出，仅移除文件末尾换行 |
| `src/webui/www/translations/webui_zh_CN.ts` | 同上，仅移除文件末尾换行 |
| `UPLOAD_RATIO_MIGRATION.md` | 表格分隔行增加规范空格，文件末尾保留一个换行 |
| `PYTHON_CI_SCOPE_FIX.md` | 表格分隔行增加规范空格 |

现有 WebUI 翻译脚本输出以 `</TS>` 结束。EditorConfig 对 TS 格式没有末尾换行要求，`end-of-file-fixer` 也已排除 TS，因此遵循该脚本的原有输出格式。

翻译文本和 XML 节点保持一致；文档内容保持一致。应用行为和两版 libtorrent 源码无需修改。

## 验证

- 两份 TS 修改前后均可解析为 XML，除文件末尾换行外字节完全一致。
- 使用与 CI 一致的 prek **0.5.10** 和根配置运行完整文件检查，应用 hook 自动修正后复跑，确认不再修改文件。
- 对本次记录文件执行相同格式检查。
- `git diff --check` 通过。

本机为 macOS，使用 Python 3.12.14，检查工具使用仓库中锁定的版本。Ubuntu 完整 File health 工作流仍需重跑。临时工具和缓存仅创建在 `/private/tmp`，验证后清理。
