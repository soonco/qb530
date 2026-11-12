# Windows C++20 哈希类型编译错误修复

日期：2026-11-12。

## 根因

Windows 使用 MSVC 14.51 和 `/std:c++20` 编译 libtorrent 2.0.15 的 `test_common` 时，在 `units.hpp` 报错：

```text
error C2039: 'result_type': is not a member of 'std::hash<int>'
```

上游 2.0.15 用 `__cplusplus < 201402` 选择 `std::hash<UnderlyingType>::result_type`。MSVC 未启用 `/Zc:__cplusplus` 时，`__cplusplus` 默认仍为 `199711L`，即使已经启用 C++20；标准库同时已经移除旧的 `result_type` typedef。因此代码走错分支，编译失败。

后续 CTest 报四个测试可执行文件不存在，是编译失败的连带结果。PowerShell 原步骤没有在 `cmake --build` 后检查退出码，继续运行了 CTest。

## 修改

| 文件 | 修复 |
|---|---|
| `2.0.15/include/libtorrent/units.hpp` | 删除依赖 `__cplusplus` 的分支，直接令 `result_type = std::size_t` |
| `.github/workflows/ci_windows.yaml` | 在 libtorrent 的 CMake 配置、编译及安装后分别检查 `$LASTEXITCODE`，失败立即退出；保留原 CTest 后检查 |

`std::hash` 返回值本来就是 `std::size_t`。修复保留该定制哈希的 `argument_type`、继承、哈希调用和结果，未修改上传统计或 public struct 布局，也不依赖重新打开标准库已移除的 typedef。

2.1.2 的同一实现已经直接使用 `std::size_t`，没有这个旧分支。本次核对并验证两版行为，不为 2.1.2 添加相同的无效改动。

## 验证

- 用 C++20 标准库加载原 2.0.15 头文件，并只在读取该头文件时模拟 MSVC 的 `__cplusplus=199711L`，复现相同 `result_type` 编译错误。
- 修复后的 2.0.15 与现有 2.1.2 在正常 C++20 宏和模拟 MSVC 宏两种情况下均通过编译、哈希结果类型/数值检查和 `unordered_map<piece_index_t, int>` 插入查询。
- 用实际 Windows workflow 步骤的 PowerShell 内容替换外部命令为受控桩，分别模拟 configure/build/ctest/install 失败：均返回原非零退出码且不执行后续阶段；全部成功时退出 0。
- 修复后重新编译 2.0.15 的上传、read_resume、resume、settings 四个测试目标。
- 2.0.15 的四个回归目标全部通过（4/4）；同轮 HTTP 修复后的 2.1.2 也通过四个目标（4/4）。包含本机 HTTP/UDP tracker、v1/v2 和上传周期尾部测试。

哈希探测日志和 PowerShell 阶段检查日志保存在工作区旁 `qb530_dependency_fix/windows-hash-probe.log`、`windows-powershell-guards.log`。编译/测试产物放在临时目录，验证后清理；本机没有 MSVC，不能把模拟宏和本地编译结果表述为已经实跑 Windows CI。

提交并推送修复后，需要用包含这些修改的新提交重新运行 Windows CI。无需取消 C++20 或跳过测试。
