# WebUI 事件监听器 ESLint 修复

日期：2026-11-12。

## 问题

WebUI CI 检查 `src/webui/www/private/views/preferences.html` 第 3638 行时，触发 `PluginQbtWebUI/require-event-parameter`。`fakeUploadRatio` 的 `change` 监听器直接引用无参数函数 `updateFakeUploadRatioRandomizationEnabled`，不符合事件处理函数必须声明事件参数的规则。

## 修改

文件：`src/webui/www/private/views/preferences.html`。

改为带 `event` 参数的箭头函数监听器，在函数体中调用原更新函数。初始化和测试仍可直接调用无参数的更新函数。倍率大于 1 时启用随机化选项、倍率为 1 时清除并禁用该选项的行为保持一致。

## 验证

- 使用仓库 `eslint.config.mjs` 对完整 `preferences.html` 运行 ESLint，检查通过。
- 使用仓库 `.jsbeautifyrc` 比较完整 HTML 与 `js-beautify` 输出，内容一致。
- `node test/testuploadpreferences.mjs` 通过：1x/5x 切换的启用、清除和不自动恢复勾选行为正常。
- `git diff --check` 通过。

检查依赖及缓存仅创建在 `/private/tmp` 的临时目录，验证后删除。本次只检查受影响的 HTML 和现有倍率功能测试；提交推送后由 WebUI CI 执行完整检查。
