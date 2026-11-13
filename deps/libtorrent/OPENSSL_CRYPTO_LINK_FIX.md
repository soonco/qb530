# Linux 静态 libtorrent 的 RAND_bytes 链接修复

日期：2026-11-13。

## 原因

`testutilsio` 的链接命令把 `libcrypto.so` 放在 `libtorrent-rasterbar.a` 之前。GNU 链接器启用 `--as-needed` 时，若处理 `libcrypto.so` 当时还没有需要它的未定义符号，就会舍弃它；随后从静态 libtorrent 中提取 `random.cpp.o`，出现 `RAND_bytes` 未解析。

两版 libtorrent 的 OpenSSL 分支只声明 `OpenSSL::SSL` 依赖，但源码直接调用 libcrypto 的函数。qBittorrent 优先使用 CMake CONFIG 模式，OpenSSL 官方配置在 Unix 共享库分支假定 SSL 的依赖由共享库隐含提供，没有把 Crypto 放入 SSL 的链接接口。它与 CMake `FindOpenSSL.cmake` 模块的 target 行为不同，因此不能依赖 SSL 间接传递 Crypto。

这也解释了同次构建中部分测试能通过：不同可执行文件提取的静态对象和直接使用的 OpenSSL 符号不同，未必都触发提前舍弃 Crypto 的问题。

来源：[OpenSSL 3.5 官方 CMake 包配置](https://github.com/openssl/openssl/blob/openssl-3.5.0/exporters/cmake/OpenSSLConfig.cmake.in)。用户日志未包含实际 OpenSSL 包配置内容；本地验证复现了与该官方配置一致的共享 target 行为。

## 修改

同步修改：

- `deps/libtorrent/2.0.15/CMakeLists.txt`
- `deps/libtorrent/2.1.2/CMakeLists.txt`

OpenSSL 分支改为：

```cmake
target_link_libraries(torrent-rasterbar PUBLIC OpenSSL::SSL OpenSSL::Crypto)
```

显式声明静态 libtorrent 对 Crypto 的直接依赖，使导出的 `LibtorrentRasterbar::torrent-rasterbar` target 为消费程序传递正确依赖，Crypto 排在静态 libtorrent 之后。没有通过关闭 `--as-needed`、添加全局链接器选项或修改随机数逻辑绕过错误。GnuTLS 分支保持一致。

## 验证

一次性 Debian Linux arm64 容器：GCC 12.2、GNU ld、CMake 3.25.1、OpenSSL 3.0.22、Boost 1.92 头文件，C++20/RelWithDebInfo。

1. 提取两版实际 CMake 的 OpenSSL 分支，编译各自实际 `src/random.cpp`。验证项目模拟 OpenSSL 官方 Unix 共享库 target 接口以及 qbt_base 的依赖顺序；错误分类代码使用局部测试桩，只为满足最小程序的其他链接符号。
2. 原依赖声明下，两版均生成 `libcrypto.so libtorrent-rasterbar.a libssl.so` 的顺序，均在 `RAND_bytes` 处链接失败。
3. 修复后两版均生成 `libtorrent-rasterbar.a libcrypto.so libssl.so` 的顺序，链接通过，实际调用 `crypto_random_bytes` 生成 32 字节的程序运行通过。
4. 配置两版完整 libtorrent 项目，确认生成的 `LibtorrentRasterbarTargets.cmake` 的 `INTERFACE_LINK_LIBRARIES` 同时含 `OpenSSL::SSL` 与 `OpenSSL::Crypto`。此配置验证对 2.1.2 关闭 WebTorrent，只核对导出依赖，没有修改其源码默认值。
5. `git diff --check` 通过。

这是实际 Linux 链接器和真实 random.cpp 的受控验证，未重新构建完整 qBittorrent、完整 libtorrent 或运行 `testutilsio` 本身。用户 CI 是 x86-64，仍需推送后重跑完整矩阵。

验证日志保存在工作区旁 `qb530_dependency_fix/openssl-link/`。临时容器及 `/private/tmp` 中的编译产物在验证后删除。

## 重跑要求

先重新配置并安装修复后的 libtorrent，再重新配置 qBittorrent，以刷新导出的 target 依赖。新 CI 任务会依次执行这些步骤；仅重试旧应用链接命令、继续使用旧的 libtorrent 安装配置不能获得此次修复。
