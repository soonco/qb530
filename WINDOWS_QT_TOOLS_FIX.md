# Windows Qt 翻译工具启动修复

日期：2026-11-12。

## 故障与定位

CMake 配置成功后，`qbt_update_translations` 调用 Qt 6.10.3 的 `lupdate.exe`，以 `3221225781`（`0xC0000135`，`STATUS_DLL_NOT_FOUND`）退出。该状态表示 Windows 加载程序时缺少 DLL，程序尚未正常启动；日志不能单独确定缺失 DLL 的完整名单。

检查发现 Windows CI 仅安装 `qtbase qtsvg qttools`，遗漏 `qtdeclarative`。Qt 6.10.3 官方 `lupdate` 构建配置在启用 QML 支持时链接 `Qt::QmlPrivate`，该运行时属于 `qtdeclarative`。官方 MSVC2022 x64 安装清单也将 `qtdeclarative` 与 `qtbase`、`qtsvg`、`qttools` 一起列为基础组件。Linux/macOS 工作流已经包含该组件。

来源：

- [Qt 6.10.3 lupdate 构建配置](https://github.com/qt/qttools/blob/v6.10.3/src/linguist/lupdate/CMakeLists.txt)
- [Qt 6.10.3 Windows 官方安装清单](https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6103/qt6_6103/Updates.xml)
- [install-qt-action v4 实现](https://github.com/jurplel/install-qt-action/blob/v4/action/src/main.ts)

## 修改

文件：`.github/workflows/ci_windows.yaml`。

1. Qt 安装的 `archives` 增加 `qtdeclarative`，提供翻译工具所需的 QML 运行时。
2. 安装后新增 `Verify Qt translation tools`，从 `QT_ROOT_DIR/bin` 依次执行 `lupdate.exe -version`、`lrelease.exe -version`。任意工具失败立即保留非零退出码退出，在编译 libtorrent 之前发现 Qt 工具启动问题。

Action 默认将 Qt 的 bin 目录添加到 PATH；原来通过绝对路径启动 `lupdate.exe` 也已找到可执行文件，因此此次修复补齐安装模块。Action 的缓存键包含 `archives`，模块列表变化会产生新的缓存键，无需关闭缓存。

Qt 版本保持 6.10.3。简体中文和英文翻译目标、应用逻辑及两版 libtorrent 源码保持一致；两个 libtorrent 矩阵任务都使用本次修复后的安装与验证步骤。

## 验证

- YAML 可解析，Qt 模块列表及新增步骤在安装 Qt 之后、构建依赖之前的位置正确。
- 使用本机 PowerShell 7.5.4 解析并执行从实际 YAML 提取的验证脚本。测试桩位于包含空格的 Qt 安装路径，确认工具路径和 `-version` 参数正确。
- 成功路径执行两个工具并返回 0；模拟 lupdate 失败时只执行 lupdate，模拟 lrelease 失败时执行两个工具；均保留各自非零退出码。
- 核对官方 Action 源码，确认 `archives` 参与缓存键，且默认设置 `QT_ROOT_DIR` 和 Qt bin PATH。
- `git diff --check` 通过。

本机是 macOS，未实跑 Windows Qt 二进制或完整 Windows 构建，需推送后重跑 CI。临时下载文件及 PowerShell 验证产物创建在 `/private/tmp`，验证后删除。
