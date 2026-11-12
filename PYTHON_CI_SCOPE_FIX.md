# Python CI 文件范围与 API 测试脚本修复

日期：2026-11-12。

## 失败原因

`CI - Python` 的辅助脚本步骤使用 `find .` 收集全部 Python 文件，只排除了搜索引擎目录。libtorrent 改为内置源码后，2.0.15、2.1.2 及其嵌套依赖的 Python 脚本也被交给应用的严格检查。mypy 首先因 `deps/libtorrent/2.0.15/setup.py` 与 `deps/libtorrent/2.0.15/bindings/python/setup.py` 对应同名模块 `setup` 而停止，后续步骤没有执行。

排除依赖源码后，本地检查进一步发现 `test/testuploadapi.py` 缺少函数类型注解、导入顺序不符合 Ruff、嵌套函数前缺空行，以及 Bandit 对测试启动程序和访问本地 HTTP 接口的报告。

## 修改

| 文件 | 调整及逻辑影响 |
|---|---|
| `.github/workflows/ci_python.yaml` | 改用 `git ls-files` 收集已跟踪的 Python 脚本，排除 `deps/**` 与 `src/searchengine/**`；避免混入依赖脚本、构建产物和本地临时文件 |
| `test/testuploadapi.py` | 为请求、偏好读取和更新函数补充参数、返回值类型；用 `UploadPreferences` TypedDict 描述读取的两个字段，使用 `cast` 标注 JSON 返回值；补空行并调整导入顺序 |
| 同上 | 对显式启动测试二进制和访问固定 localhost HTTP 接口的三处调用，添加有原因说明的局部 `nosec B404`、`B603`、`B310` 标记 |

mypy 与 Pyright 仍使用原有严格检查；没有增加全局忽略项。两版 libtorrent 源码及其构建测试未修改，依赖脚本保留各自工具配置。搜索引擎仍使用工作流后半段的独立检查。

API 测试的请求地址、子进程参数、断言和配置持久化检查保持一致。类型注解及 `cast` 不增加运行时转换。

## 验证

从修改后的 YAML 提取并执行实际文件收集步骤，确认收集到以下 6 个脚本，包含我们新增的测试，不包含两版 libtorrent 或搜索引擎文件：

- `.github/workflows/helper/pre-commit/check_grid_items_order.py`
- `.github/workflows/helper/pre-commit/check_translation_tag.py`
- `.github/workflows/helper/pre-commit/check_webui_translation_tag.py`
- `dist/windows/gather_qt_translations.py`
- `src/webui/www/tstool.py`
- `test/testuploadapi.py`

按 YAML 保存的命令执行全部辅助脚本检查：

- `mypy --strict`：6 个文件通过。
- `pyright --project src/searchengine/nova3/pyproject.toml`：0 个错误、警告。
- Ruff、Pyflakes、Bandit、pycodestyle：全部通过。
- `python -m compileall`：6 个文件通过。
- `git diff --check`：通过。

本地验证使用 macOS Python 3.12.14；用户日志中的 Linux runner 使用 Python 3.15.2，仍需推送后由 CI 验证该环境。工具版本为 mypy 2.4.1、Pyright 1.1.414、Ruff 0.17.4、Pyflakes 4.1.0、Bandit 1.9.4、pycodestyle 2.15.0。

本次未构建或运行 qBittorrent 二进制，因此未重新运行 API 集成测试。检查工具、类型检查缓存与字节编译产物全部放在 `/private/tmp` 临时目录，验证后删除。
