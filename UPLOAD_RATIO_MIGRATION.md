# qBittorrent 5.3.0 上传倍率迁移记录

日期：2026-11-12。依据：`../qb523_migration_analysis/README.md` 的 L01–L12。
本次已完成源码适配；下面分别记录通过的验证与未完成的跨平台/可选组件验收，不把源码完成等同于所有平台发行验收完成。

后续交付修复：两版 libtorrent 已改为父仓库内的普通源码目录，解决初始化提交中缺 `.gitmodules` URL 的 gitlink 错误。下文关于独立嵌套仓库的描述记录的是迁移当时状态，当前交付方式见 `deps/libtorrent/README.md`。

## 1. 目标基线与范围

| 项目 | 版本 | 原始 commit |
|---|---|---|
| qBittorrent：`qb530` | release-5.3.0 | `9213075dd5f7b352208361c6a84c1c115efc898e` |
| `deps/libtorrent/2.0.15` | v2.0.15 | `1eb18faeae156d8dbbab42935c082f8b81f50989` |
| `deps/libtorrent/2.1.2` | v2.1.2 | `6da363d2994f17c0b3c0450d124cf73a31a73847` |

没有降级到文档的 2.0.11，也没有恢复 1.x。开始时两份 libtorrent 无本地功能改动。
原有 workflow 本地依赖配置、187 个其他语言/帮助文件删除及中英文打包修改均保留。
本次未提交 Git commit，未发布发行包。

## 2. 功能与职责映射

| 契约 | 实现位置与行为 | 验证 |
|---|---|---|
| L01–L03 | Session 接口、CachedSettingValue、loadLTSettings、configureDeferred；默认 1/true，应用夹到 1–5，引擎只限制最低 1 | nox API 实测默认、越界、独立部分更新、保存；引擎线程测试即时应用 settings 后只影响后续累计 |
| L04–L05 | 两版 torrent 的 second_tick 调用 account_uploaded_payload；整数均匀抽样 1..R，64 位乘法，同批更新三个累计 | 固定 2→5→1、空 tick、可重复 RNG、协议字节排除、1GB×5 测试通过 |
| L06、L09 | all_time_upload 保持倍率历史；all_time_real_uploaded 独立真实历史；m_stat/peer/session 网络计数保持真实 | 源码数据流核对，历史计数及 payload 测试通过；分享限制动作的真实任务联调未执行 |
| L07 | tracker uploaded 读取当前 announcing 周期累计；start guard 之后清周期，不清历史 | 本地 HTTP/UDP、两 tracker、v1/v2、停止尾部和新周期快照测试通过；2.1.2 构造请求直接断言也通过 |
| L08 | add_torrent_params、resume read/write、兼容复制、构造初始化、状态和保存链 | 新键往返、大整数、缺键 fallback、保存尾部和去重测试通过；qBittorrent 两存储后端调用 LT bencode 原样保留键 |
| L10 | 桌面和 WebUI 高级设置增加 1x–5x；1x 清空并禁用随机化，切回不恢复；保存 ratio>1 && checked | WebUI 实际嵌入脚本联动测试通过；桌面完成编译，尚未人工 GUI 操作验收 |
| L11 | 同次 tracker_announce_alert 带真实周期/汇报周期快照；不相等输出 INFO，继续原 tracker 状态处理 | 本地 announce alert 与请求路径测试通过，应用日志接线源码核对 |
| L12 | 自定义 ABI 宏、导出探测函数、CMake 编译/链接探测、启动校验；CI 仍选本地两版源码 | 两版引擎与应用编译通过；本机旧头文件混入被探测拒绝；Windows/Linux job 未实际运行 |

计数语义：

- `m_total_uploaded / all_time_upload / total_uploaded`：倍率后的历史上传，跨周期与重启保存。
- `m_total_real_uploaded / all_time_real_uploaded / total_real_uploaded`：真实历史 payload。
- `m_total_tracker_uploaded`：当前 announcing 周期的倍率上传，不持久化。
- `m_accounted_payload_upload`：当前真实 payload 快照的已结算基线，用来防止生命周期 flush 后 tick 重复记账。
- `m_stat.total_payload_upload()`：真实周期 payload；上传速度、peer、全局流量沿上游口径。
- alert 的 `total_real_uploaded` 是真实**周期**快照，与同名 resume 字段的真实**历史**累计不同。

应用键：`BitTorrent/Session/FakeUploadRatio`、`BitTorrent/Session/FakeUploadRatioRandomization`。
API 键：`fake_upload_ratio`、`fake_upload_ratio_randomization`。
UI 加载不回写；后端允许 `(1,true)`；API 只更新传入字段。倍率关闭不回算历史、不清周期、不主动 announce。
启动读取旧配置的 clamp 沿 CachedSettingValue 既有机制，不由该功能主动改写存储原值。

## 3. 本次修改的应用文件

| 文件 | 修改内容 |
|---|---|
| `src/base/bittorrent/session.h` | 4 个倍率/随机化虚接口 |
| `src/base/bittorrent/sessionimpl.h` | override 和两个缓存设置 |
| `src/base/bittorrent/sessionimpl.cpp` | 默认/校验/保存/热更新、引擎下发、ABI 启动探测、announce 诊断 |
| `src/gui/advancedsettings.h` | 倍率下拉框、随机化 checkbox 成员 |
| `src/gui/advancedsettings.cpp` | 高级设置末尾新分区、加载、联动、保存 |
| `src/webui/api/appcontroller.cpp` | preferences 响应和 setPreferences 部分更新 |
| `src/webui/www/private/views/preferences.html` | 新 fieldset、加载/保存、change 事件和联动 |
| `src/lang/qbittorrent_en.ts`、`qbittorrent_zh_CN.ts` | 4 条设置文本及 1 条日志文本 |
| `src/webui/www/translations/webui_en.ts`、`webui_zh_CN.ts` | 4 条设置文本 |
| `cmake/Modules/CheckPackages.cmake` | 依赖发现后执行功能探测 |
| `cmake/Modules/CheckLibtorrentUploadAccounting.cmake`（新增） | C++20 下同时检查 settings、public 字段与链接符号，拒绝官方未定制库 |
| `.github/workflows/ci_macos.yaml`、`ci_ubuntu.yaml`、`ci_windows.yaml`、`coverity-scan.yaml` | 保留本地 source_dir；启用并运行上传、resume、settings 四个测试目标；Windows 显式传播测试失败状态 |
| `test/testuploadpreferences.mjs`（新增） | 执行实际嵌入 WebUI 脚本的无依赖 Node 联动测试 |
| `test/testuploadapi.py`（新增） | 隔离 profile、本地 nox API 与配置保存测试 |

`TorrentImpl`、torrents API 序列化和全局流量逻辑无需改写：现有历史上传出口自动继承倍率语义。
对应消费者已核对：`TorrentImpl::totalUpload/realRatio/eta/popularity`、`SessionImpl::processTorrentShareLimits`、
`serialize_torrent.cpp` 的 uploaded/ratio、`torrentscontroller.cpp` 的 total_uploaded/平均上传速度，
以及 LT 的 `seed_rank`、share mode 历史上传门槛。继续使用 5.3.0 上游公式。
普通 GUI/API 未新增真实/汇报双统计字段，WebAPI 版本未变。

## 4. 两版 libtorrent 文件清单

下列每项均分别修改 `deps/libtorrent/2.0.15/` 和 `deps/libtorrent/2.1.2/` 下的文件：

| 相对路径 | 修改内容 |
|---|---|
| `include/libtorrent/settings_pack.hpp` | 在各自 bool/int 枚举尾部追加设置；不硬编码旧编号 |
| `src/settings_pack.cpp` | 对应索引默认 true/1 |
| `include/libtorrent/add_torrent_params.hpp` | 新真实历史上传字段，直接构造默认 0 |
| `include/libtorrent/torrent_status.hpp` | 新真实历史状态字段 |
| 2.0.15：`include/libtorrent/torrent.hpp`；2.1.2：`include/libtorrent/aux_/torrent.hpp` | 真实历史、周期汇报、结算基线、结算方法；内部 get_resume_data 改非 const 以结算保存尾部 |
| `src/torrent.cpp` | 构造/初始化、统计 tick、生命周期 flush、请求快照、保存与状态导出 |
| `include/libtorrent/alert_types.hpp`、`src/alert.cpp` | announce 构造参数和两个周期快照 |
| `src/read_resume_data.cpp` | 缺 total_real_uploaded 时回退 total_uploaded |
| `src/write_resume_data.cpp` | 序列化真实历史新键 |
| `src/session_handle.cpp` | 仍存在的旧 ABI resume 合并路径复制新字段 |
| `bindings/python/src/alert.cpp` | 两个只读 announce 字段 |
| `bindings/python/src/session.cpp` | ATP 真实历史读写字段 |
| `bindings/python/src/torrent_status.cpp` | 状态真实历史只读字段 |
| `include/libtorrent/version.hpp`、`src/version.cpp` | LIBTORRENT_FAKE_UPLOAD_ABI=1 和 fake_upload_abi_v1 导出函数 |
| `test/test_upload_accounting.cpp`（新增）、`test/Jamfile` | 5 个专项用例；CMake 自动发现，Jamfile 显式注册 |
| `.gitignore` | 忽略本地 /build/ 产物 |

新增字段会改变 public struct ABI，应用、库和可选 Python 扩展必须一起重建。
版本号仍保持原上游版本，自定义 ABI 宏/符号负责区别功能；宏检查头文件，导出函数同时检查链接库，启动调用检查运行来源。
本次本地应用使用静态 libtorrent，构建链接表分别指向 `build/lt-2.0.15/lib/libtorrent-rasterbar.a` 和
`build/lt-2.1.2/lib/libtorrent-rasterbar.a`，运行时没有另行动态加载系统 libtorrent。

### 版本适配差异

- 2.0.15 仍在 `announce_with_tracker()` 构造请求和 alert。
- 2.1.2 已拆为 `build_tracker_request()` / `send_tracker_request()`；普通 announce 和指定 tracker 路径共用，统一修改后两条路径都覆盖。
- 2.1.2 的协议 version 已在 tracker alert 基类，保留现有结构，不重复添加旧派生字段。
- 2.1.2 内部 torrent 移到 aux_ 命名空间及头文件，按其现有位置适配。
- 2.1.2 新增 WebSocket/WebTorrent：编码继续读取统一 req.uploaded。源码核对完成；本地未做 WebSocket/HTTPS 汇报抓包。
- 两版均保留上游 peer tick、状态更新、下载累计及 stat.second_tick 顺序。
- 新 setting 放在各版本当前末尾，保留 2.1.2 WebTorrent 等上游设置。
- 原始 clone 缺子模块。已按各仓库锁定提交初始化两版 try_signal；2.1.2 还初始化 libdatachannel 及其 json/libjuice/libsrtp/plog/usrsctp 子模块。未替换模拟器或二进制 fixture。

## 5. 单独记录的边界修复

### 暂停/保存尾部（L05/L07/L08）

直接迁移旧 tick 逻辑后，真实计数测试复现：已结算 360 字节，再发送 7 字节立即 pause，然后经过 paused tick，
历史真实仍为 360 而不是 367，汇报历史也缺 7。该复现使用网络线程上的真实 torrent/stat 对象注入 payload，不是实际 peer 文件传输实验。

修复方法：

1. 以当前真实 payload 累计减去已结算基线取得 delta；正常 tick 仍每 torrent 抽样一次，空 tick 保留 RNG 消耗。
2. stop_announcing、开始新周期清 stat 前、paused tick、save_resume_data/get_resume_data 在存在未结算字节时结算。
3. flush 使用当时有效倍率；随机模式的非空尾部额外抽样一次，零尾部不抽样；立即推进基线。
4. 新周期把周期汇报和基线清 0，保留两类历史；普通 announce 不 flush、不重复乘倍。
5. save/get_resume_data flush 避免关闭保存早于 stop 时遗漏尾部；条件保存前先标记计数变化。
6. abort 走 stop_announcing，测试确认最后尾部计入；后续 tick 不重复结算。

这相对旧实现在生命周期/保存边界新增非空结算批次，可能改变随机序列和累计比例，明确属于修复差异。
正常两 tick 之间的普通 announce 仍是已结算汇报快照，1x 也可能暂时小于实时 payload。
没有宣称解决不可还原的旧历史：缺真实键仍按旧 total_uploaded 初始化，绝不按当前倍率反推。

### 日志文本（L11）

将旧 `Tracker announce sent / Fake uploaded` 改为 `Tracker announce attempted / Reported uploaded`，并补中英文翻译。
表达的是尝试和整个汇报累计，不表示 tracker 成功或额外虚构差额。触发条件仍为两快照不相等。

### WebUI 异常值

加载倍率额外检查 Number.isInteger，拒绝非整数/缺字段并回退 1，避免 select 得到不存在的选项。
正常 API 整数及旧版缺字段语义保持一致。

## 6. 已执行的验证

本机 macOS arm64，AppleClang 21、Boost 1.92、Qt 6.11.1；引擎使用 Release、C++20、静态库、deprecated-functions=OFF。
2.1.2 保持 WebTorrent 开启并实际编译 libdatachannel。使用工作区内安装前缀，未安装到系统 libtorrent 目录。

| 验证 | 结果 |
|---|---|
| 两版完整 libtorrent 编译 | 通过 |
| 两版上传专项 5 个用例 | 通过：生命周期、大值、RNG、resume、HTTP/UDP 多 tracker/v1/v2 快照、保存/abort 尾部 |
| 两版上游 test_read_resume、test_resume、test_settings_pack | 各 3 个测试目标通过 |
| qBittorrent + 2.0.15 GUI=ON | 编译/链接通过；22 项应用测试通过 |
| qBittorrent + 2.1.2 GUI=OFF | 编译/链接通过；22 项应用测试通过 |
| 最新 nox 二进制 API 本地运行 | 默认 1/true、99→5、-3→1、部分更新、(1,true)、持久化通过 |
| WebUI 实际内嵌 JS | 1→5→1→5 清空/禁用/不恢复通过 |
| 四份 TS | XML 检查及 lrelease 编译通过 |
| 三个仓库 git diff --check | 通过 |
| 旧 libtorrent 混入 | configure 功能探测正确拒绝；显式 SDK 路径排除本机 /usr/local/include 的旧头文件 |
| 官方 fixture/其他语言 | 未引入文档里的二进制换行改动；仍只保留 en/zh_CN |

本地 tracker 验证需要回环端口监听，沙箱禁止 bind，已在获准的沙箱外运行。
测试先等待 started 成功响应再发 stopped，并等 stopped 响应后开始下一周期，遵守目标上游状态机。
验证例：真实周期 107 → 汇报 535；下一周期真实 10 → 汇报 50，两 tracker×两哈希各四次快照一致。
HTTP 请求由测试服务器接收；UDP 测试 tracker 返回响应。测试未访问公共 tracker。

日志位置（构建目录属于忽略文件）：

- `deps/libtorrent/<版本>/build/migration-ctest.log`：引擎初轮全部回归通过。
- `deps/libtorrent/<版本>/build/migration-tracker-ctest.log`：最终 5 项专项用例及本地 tracker 通过。
- `deps/libtorrent/<版本>/build/migration-final-build.log`、`migration-tracker-build.log`：编译日志。
- `/private/tmp/qbt-2.0.15-ctest.log`、`/private/tmp/qbt-2.1.2-ctest.log`：各 22 项应用测试。
- `/private/tmp/qbt-<版本>-build.log`、`qbt-<版本>-final-build.log`：应用编译/重链接。

部分中间日志保留了修复前或测试准备阶段的失败，请以最终日志和此表为准。

## 7. 验证范围与交付注意事项

- Windows/Linux/Coverity job 和 macOS 的另外两种 GUI/nox 组合未在本机执行；workflow 已接入四个引擎测试目标。
- Python 三个绑定文件已同步，但本机缺 Boost.Python，未编译/运行 Python 扩展。
- GUI 二进制已编译，offscreen 复用 nox API 启动测试超时，未取得桌面运行验收证据；没有把它列为通过。
- 未执行真实 peer 长时传输、分享限制删除/停止动作、实际磁盘重启、HTTPS/WebSocket 抓包、发行包签名/安装验收。计数测试通过并不替代这些发布验收。
- 本机实际应用动态依赖由 Homebrew 解析到 OpenSSL 4、Qt 6.11.1；这是本地验证产物，未当作使用 CI Qt 6.10.3 的正式发行包。
- `deps/libtorrent/*` 仍是独立嵌套 Git 仓库，父仓库显示 `?? deps/`。交付/提交时必须连同两份定制源码及锁定子模块一起保存；仅提交父仓库 workflow 不会携带这些修改。不要误把原始 tag 的 gitlink 当作已包含定制补丁。
- 顶层 CMake 继续发现已安装依赖，不自动 add_subdirectory 内置库；必须先构建并安装选定定制库，再指定 LibtorrentRasterbar_DIR。

## 8. 复验命令

在 qb530 根目录，任选 `v=2.0.15` 或 `v=2.1.2`：

```sh
v=2.0.15
cmake -S "deps/libtorrent/$v" -B "deps/libtorrent/$v/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 -DBUILD_SHARED_LIBS=OFF \
  -Ddeprecated-functions=OFF -Dbuild_tests=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/lt-$v"
cmake --build "deps/libtorrent/$v/build" --target torrent-rasterbar \
  test_upload_accounting test_read_resume test_resume test_settings_pack
ctest --test-dir "deps/libtorrent/$v/build" \
  -R '^(test_upload_accounting|test_read_resume|test_resume|test_settings_pack)$' --output-on-failure
cmake --install "deps/libtorrent/$v/build"
cmake -S . -B "build/qbt-$v" -G Ninja -DGUI=OFF -DTESTING=ON \
  -DLibtorrentRasterbar_DIR="$PWD/build/lt-$v/lib/cmake/LibtorrentRasterbar"
cmake --build "build/qbt-$v"
ctest --test-dir "build/qbt-$v/test" --output-on-failure
node test/testuploadpreferences.mjs
```

macOS 本机需显式 `CMAKE_OSX_SYSROOT` 指向当前 Xcode SDK，避开默认搜索到的旧 /usr/local/include/libtorrent。
OpenSSL/Qt 位置按实际工具链指定；依赖来源错误会由配置探测拒绝。
nox API 测试命令：

```sh
QT_PLUGIN_PATH=/opt/homebrew/share/qt/plugins \
  python3 test/testuploadapi.py build/qbt-2.1.2/qbittorrent-nox.app/Contents/MacOS/qbittorrent-nox
```

API 测试只使用临时 profile 和回环地址，finally 终止本次启动的进程，不修改用户现有配置。

