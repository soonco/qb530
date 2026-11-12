# Unicode 双向控制字符编译修复

日期：2026-11-12。

## 问题

Linux GCC 在编译 `peerinfo.cpp`、`sessionimpl.cpp` 等包含公共头文件的源码时，发现 `src/base/unicodestrings.h` 的乌兹别克语名称中有直接写入的不可见 `U+200E`（LEFT-TO-RIGHT MARK）。`-Wbidi-chars` 的诊断在 CI 的 `-Werror` 下成为编译错误。

虽然应用的可选翻译已精简为简体中文和英语，公共语言名称函数 `Utils::Misc::languageToLocalizedString` 仍引用该常量；翻译资源的精简不会阻止编译器读取它。

## 修改

文件：`src/base/unicodestrings.h`。

把 `C_LOCALE_UZBEK` 字符串末尾的原始控制字符改为显式 `\u200E` 转义。源码中不再含该不可见字符，字符串在运行时仍包含原来的字符序列，名称显示逻辑保持一致。没有关闭或降低 GCC 警告，也没有修改语言选择、翻译资源或两版 libtorrent。

## 验证

- 对比修改前后的字符串码点，均为 `U+0623 U+06C7 U+0632 U+0628 U+06D0 U+0643 U+200E`。
- 扫描 559 个应用与测试 C++ 文件（排除第三方源码），没有发现原始 Unicode 双向控制字符。
- 使用本机 Apple Clang、C++20、Qt 6.11.1、`-Wall -Wextra -Werror -pedantic-errors` 编译包含实际头文件的临时程序，通过；运行时字符串与显式码点序列比较通过。
- `git diff --check` 通过。

本机未安装 GNU GCC，因此未实跑 Linux 的 `-Wbidi-chars` 或完整应用构建；需推送后由 CI 验证。临时验证文件和可执行程序创建在 `/private/tmp`，验证后删除。
