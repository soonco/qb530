# Windows 自定义上传统计探测修复

日期：2026-11-12。

## 现象与诊断边界

应用 CMake 找到本地安装的 libtorrent 2.1.2 后，`QBT_HAS_UPLOAD_ACCOUNTING` 编译/链接探测失败。原提示把失败统一描述为“需要定制 libtorrent”，没有打印该探测的实际编译或链接错误。随后 Windows 步骤还继续调用 Ninja，因配置未完成而报 `build.ninja` 不存在。

日志中的 CMP0144、CMP0167 是依赖发现的 policy warning；日志已经显示 Boost 找到，不能据此认定它们是致命原因。用户提供的输出也没有 `QBT_HAS_UPLOAD_ACCOUNTING` 对应的 `error C...` / `LNK...`，因此无法仅凭现有日志确定 runner 上唯一的失败原因。

检查发现原探测存在实际配置缺口：

1. `try_compile` 不自动继承单配置主项目的 `CMAKE_BUILD_TYPE`。应用和静态依赖为 `RelWithDebInfo`，临时探测可能使用空/default 配置。在 MSVC 上可能选择不同 CRT 或 STL 调试设置。
2. `CheckPackages` 先于 `CommonConfig` 执行，探测没有获得应用正常目标的 MSVC `/Zc:__cplusplus`、`/permissive-`、`/utf-8`、Windows SDK 宏及运行库选项。
3. 原 `check_cxx_source_compiles` 的详细失败输出没有直接展示；实际诊断只在 CMake 日志中。

## 修改

| 文件 | 调整 |
|---|---|
| `cmake/Modules/CheckLibtorrentUploadAccounting.cmake` | 改用兼容项目最低 CMake 版本的 `try_compile(... OUTPUT_VARIABLE ...)`，保存并在失败时打印具体错误 |
| 同上 | 显式要求 C++20、无编译器扩展、生成可执行文件以保证真正链接；用主项目 build type 设置探测配置 |
| 同上 | MSVC 探测使用与应用一致的运行库选择、编译选项和 Windows 10 相关宏 |
| `.github/workflows/ci_windows.yaml` | 修正 CXXFLAGS 追加时的空格；应用配置、翻译生成、编译和 check 后分别检查退出码，失败不再继续 |

保留原自定义宏、两项 settings、ATP/status/alert 字段和 `fake_upload_abi_v1()` 链接检查。探测仍要求真实链接成功，不能用仅构建静态目标或删除探测绕过 ABI 检查。函数内设置不改写调用者的编译配置；已有显式 `CMAKE_TRY_COMPILE_CONFIGURATION` 选择也保留。

探测日志写到 `build/CMakeFiles/qbt_upload_accounting.log`，包含依赖目录/版本、配置、运行库、CXXFLAGS 和真实编译/链接输出。失败时同样打印到终端，使下一次 CI 能直接显示具体原因。

## 验证

- 受控 release-only 静态库：原探测因空配置选择另一组符号而失败，新探测按 `RelWithDebInfo` 通过，复现并验证配置错配修复。
- 缺自定义宏、缺链接探测函数分别仍拒绝，终端包含具体编译或链接错误。
- 检查函数作用域：探测没有改变调用者的 CXXFLAGS 或显式配置选择。
- 使用实际 Windows 应用步骤的 PowerShell 内容和受控命令桩，分别模拟 configure/translations/build/check 失败，均保留非零退出码并停止后续阶段；成功路径仍执行全部步骤。CXXFLAGS 追加结果包含正确分隔空格。
- 两份实际定制 libtorrent 均以 RelWithDebInfo 静态构建并安装到临时前缀，然后使用安装包导出的 CMake target 运行新探测，2.0.15 和 2.1.2 都成功；2.1.2 保持 WebTorrent 默认开启并包含其静态依赖。
- 同一消费方构建目录先使用 2.0.15，再切换为 2.1.2，探测都重新执行并成功；显式 Release 多配置选择保留。调用者指定 STATIC_LIBRARY 探测模式时仍强制真实可执行链接，缺自定义符号正确失败。

受控验证和实际安装库的验证日志保存在工作区旁 `qb530_dependency_fix/`。本机是 macOS，不等于已实跑 Windows MSVC。提交并推送后，应使用新提交重跑 Windows CI；若仍失败，新日志会显示具体 `error C...` / `LNK...`，可据此继续定位。
