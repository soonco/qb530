# Ubuntu AppImage SQL 驱动打包修复

日期：2026-11-13。

## 故障原因

此次失败发生在 `Package AppImage` 的 `linuxdeploy --plugin qt` 阶段，程序已经编译、安装到 AppDir。日志中的无 GUI 构建使用 Qt 6.6.3 和 libtorrent 2.0.15。

```text
Deploying Qt sqldrivers plugins
Deploying .../plugins/sqldrivers/libqsqlmysql.so
ERROR: Could not find dependency: libmysqlclient.so.21
ERROR: Failed to run plugin: qt (exit code: 1)
```

Qt 打包插件从 AppDir 中发现 `QtSql` 后，会遍历 Qt 安装中的整个 `sqldrivers` 目录并部署驱动及其依赖。原有工作流只删除 `libqsqlmimer.so`，仍然尝试部署 MySQL、PostgreSQL、ODBC 等驱动，遇到未安装的数据库客户端库就中止打包。

源码中的 `QSqlDatabase::addDatabase()` 均指定 `QSQLITE`，位于 `src/base/bittorrent/dbresumedatastorage.cpp` 和 `src/base/keyvaluedatastorage.cpp`。Windows 打包步骤也只复制 `qsqlite.dll`。

已核对与日志一致的 Qt 打包插件提交 `9b9fca179e7312ae4a433ad6c3183d73c116180b`：SQL 部署器直接调用 `deployStandardQtPlugins({"sqldrivers"})`，通用部署器遍历整个目录，仅跳过 `.debug` 文件；该版本没有提供按 SQL 驱动筛选的配置。

参考源码：

- [SQL 部署器](https://github.com/linuxdeploy/linuxdeploy-plugin-qt/blob/9b9fca179e7312ae4a433ad6c3183d73c116180b/src/deployers/SqlPluginsDeployer.cpp)
- [通用插件部署器](https://github.com/linuxdeploy/linuxdeploy-plugin-qt/blob/9b9fca179e7312ae4a433ad6c3183d73c116180b/src/deployers/BasicPluginsDeployer.cpp)

## 修改内容

修改文件：`.github/workflows/ci_ubuntu.yaml`，`Package AppImage` 步骤。

1. 使用 `qmake -query QT_INSTALL_PLUGINS` 获取实际 Qt 插件目录。
2. 打包前检查 `sqldrivers/libqsqlite.so` 存在，缺失时立即失败。
3. 在本次 CI 使用的 Qt 安装目录中，删除 `libqsql*.so` 中除 `libqsqlite.so` 以外的 SQL 驱动，替换原来仅删除 Mimer 的命令。
4. Qt 部署完成后检查 `qbittorrent/usr/plugins/sqldrivers/libqsqlite.so` 存在，避免生成缺少 SQLite 驱动的包。

该步骤没有矩阵条件，适用于 libtorrent **2.0.15、2.1.2** 的 **GUI=ON、GUI=OFF** 全部四个组合。应用和 libtorrent 源码无需调整；Qt 网络、TLS、图像及平台插件继续由原有 Qt 打包流程处理。

## 验证

- Ruby YAML 解析成功，并从实际工作流提取 `run` 脚本；`bash -n` 通过。
- 使用临时 Qt 插件目录和 linuxdeploy 测试桩执行该脚本，采用与 GitHub Actions 相同的 `bash -e`。插件路径包含空格，并覆盖 SQL 插件符号链接。
- 原命令在模拟缺少其他 SQL 驱动依赖时返回 1；修改后只部署 SQLite，执行两次 linuxdeploy，复制 AppRun hook，并成功生成模拟输出文件。网络和 TLS 插件保持可部署。
- SQLite 源文件缺失时在调用 linuxdeploy 之前失败；部署后 SQLite 缺失时在生成 AppImage 之前失败。
- 模拟 `qmake`、Qt 插件、AppImage 输出工具失败，分别保留退出码 23、17、19，未继续生成输出。
- `git diff --check` 通过。

本机是 macOS，以上为真实工作流脚本的受控验证，未执行 Linux ELF 依赖部署或完整 AppImage 构建。真实产物仍需重跑 Ubuntu CI 验证；上述验证不代表所有其他 Qt 插件依赖都已在 runner 上满足。临时验证文件位于 `/private/tmp`，验证后清理。
