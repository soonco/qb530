# HTTP 测试服务器启动超时修复

日期：2026-11-12。同步修改 libtorrent 2.0.15 与 2.1.2。

## 故障与定位

2.1.2 的 `upload_accounting_tracker_snapshots` 启动 `web_server.py` 后等待 `LISTENING_PORT`。CI 中依次尝试 `python3` 和 `python`，均在 30 秒内没有收到端口，最终抛出 `failed to spawn web_server.py: exhausted all python interpreters`。

脚本已经进入本次源码交付，不是遗漏 Python 文件。检查 Python 标准库发现 `HTTPServer.server_bind()` 先调用 `TCPServer.server_bind()` 绑定地址，再通过 `socket.getfqdn(host)` 设置服务器名，之后才会 listen 并进入脚本的端口报告逻辑。

2.1.2 原来使用 `0.0.0.0`，`getfqdn()` 会转换为本机主机名并查询其 DNS；runner 主机名解析阻塞会卡在报告端口之前。2.0.15 的 `127.0.0.1` 也经过同一查询。本次通过模拟阻塞 `socket.getfqdn()`，对两个原始脚本均复现“服务器尚未就绪就等待 DNS”的路径。该复现说明源码中的阻塞点与 CI 超时吻合；现有 CI 日志未包含 Python 堆栈，不能直接证明 runner 当时的 DNS 状态。

## 修复

两份 `test/web_server.py` 都新增 `server_bind()` override：

```python
TCPServer.server_bind(self)
self.server_name, self.server_port = self.server_address[:2]
```

服务器名取已经绑定的地址，避免启动时查询 DNS。保留各版本的绑定地址、端口选择方式、TLS 配置、请求处理和 tracker 响应。2.1.2 仍在完成服务器初始化后报告 `LISTENING_PORT`；2.0.15 仍使用原 TCP 端口就绪检查。

不通过增加超时、跳过 HTTP 测试或替换 Python 解释器掩盖问题。上传统计业务代码没有修改。

## 验证

使用真实脚本、临时工作目录和本机 HTTP 请求，测试时将 `socket.getfqdn()` 替换为等待 30 秒的函数：

- 修复前两版均进入该函数，尚未报告端口或进入可服务状态，成功复现阻塞路径。
- 修复后两版均没有调用该函数，分别约 0.11 秒和 0.07 秒完成启动及 HTTP tracker 请求，响应状态 200，返回预期 bencode。
- 两份 Python 文件语法解析及 `git diff --check` 通过。
- 两版都重新编译四个回归目标并运行 `test_upload_accounting`、`test_read_resume`、`test_resume`、`test_settings_pack`，各 4/4 通过；上传专项保留 HTTP/UDP、v1/v2 和周期尾部验证。

专项复现脚本保存在工作区旁的 `qb530_dependency_fix/test_web_server_startup.py`；构建和测试日志保存在同一目录。验证产生的编译缓存使用 `/private/tmp`，不写入 qb530 的 build 目录。

本机 macOS arm64 使用 Python 3.9.6、CMake 4.3.4、Boost 1.92.0 和 OpenSSL 3，与远程 CI 工具链不同。提交并推送修复后，需要在包含这次改动的新提交上重跑原 CI；旧提交仍使用原脚本。
