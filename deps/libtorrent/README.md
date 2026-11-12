# libtorrent 源码交付与子模块错误修复

日期：2026-11-12。

## 原因

父仓库的初始化提交把 `2.0.15` 和 `2.1.2` 记录为 mode `160000` 的 gitlink，但父仓库没有对应 `.gitmodules` URL。GitHub Actions checkout 清理或处理这些记录时会出现：

```text
fatal: No url found for submodule path 'deps/libtorrent/2.0.15' in .gitmodules
```

两个 gitlink 指向官方提交，只包含一个提交号，没有保存本地定制文件。补上官方仓库 URL 虽能提供子模块来源，检出仍会得到官方代码，无法满足应用的自定义上传统计 ABI。

## 修复后的目录形式

本项目直接保存两版定制源码和现有构建依赖，使用普通 Git 文件，不使用 libtorrent 子模块。

| 目录 | 官方基线 | 原始 commit |
|---|---|---|
| `2.0.15/` | `v2.0.15` | `1eb18faeae156d8dbbab42935c082f8b81f50989` |
| `2.1.2/` | `v2.1.2` | `6da363d2994f17c0b3c0450d124cf73a31a73847` |

两版现有的上传倍率、真实历史统计、resume、alert、Python 绑定及测试代码全部保存。依赖目录内的 `.gitmodules` 保留为上游来源资料，不对应父仓库中的 gitlink，也不需要执行 `git submodule update`。

已有内部依赖也作为普通源码保存：

| 依赖 | 锁定 commit |
|---|---|
| 两版 `deps/try_signal` | `105cce59972f925a33aa6b1c3109e4cd3caf583d` |
| 2.1.2 `deps/libdatachannel` | `a02b751917ac8afc8c58dc6f4461d25ff9465d48` |
| libdatachannel `deps/json` | `55f93686c01528224f448c19128836e7df245f72` |
| libdatachannel `deps/libjuice` | `5948a4162d37bc213d6051b67ee2876ccc5a99a6` |
| libdatachannel `deps/libsrtp` | `ee1a77c9f9dc02c42bda9901038c500c5efe4cfa` |
| libdatachannel `deps/plog` | `94899e0b926ac1b0f4750bfbd495167b4a6ae9ef` |
| libdatachannel `deps/usrsctp` | `fec583d54493f879d2ae44a743423bf8a04371ab` |

未初始化的 `deps/asio-gnutls` 和 `simulation/libsimulator` 仍未纳入源码。本项目当前 CI 构建选项不依赖它们；以后启用对应可选组件时需补齐锁定源码。

## 本次修改

1. 从父仓库索引移除两个 gitlink，将两版目录及已有内部依赖纳入普通文件跟踪。
2. 移走 8 个已经指向不存在 Git 元数据的内部 `.git` 文件。备份保存在工作区旁的 `qb530_dependency_fix/git-metadata/`，不删除依赖源码。
3. 父仓库 `.gitattributes` 对 `deps/libtorrent/**` 设置 `-text`，避免全局文本换行规则转换上游二进制 fixture、示例视频及源码原始字节。
4. 父仓库 pre-commit 排除 `deps/libtorrent/`，沿用原先不扫描外部依赖的范围，避免格式化工具改写内置上游代码。
5. 对全部 9,022 个原始源码/资源文件保存 SHA-256 快照，并校验暂存内容与干净检出内容。快照保存在工作区旁的 `qb530_dependency_fix/SOURCE_SNAPSHOT.json`。没有纳入 `.DS_Store`、Git 元数据或编译缓存。

CI 继续使用 `deps/libtorrent/${{ matrix.libtorrent.source_dir }}`，两个 `source_dir` 仍为 `2.0.15` / `2.1.2`。普通 checkout 即可获得现有构建所需源码；无需恢复 clone 官方 tag 的安装步骤。

## 提交与后续检出

本修复需要作为新的提交包含两个 gitlink 的删除、全部普通依赖文件、`.gitattributes`、`.pre-commit-config.yaml` 和本文档。修复暂存在本地索引中；本次没有创建或推送父仓库提交。

完成提交并推送后，用修复后的提交重新运行 CI。初始化提交本身仍含错误 gitlink，对旧提交重新运行任务仍会复现原错误。

维护源码时必须同时提交目录内的实际文件修改。不要再把这些目录重新 `git init` 或作为嵌套仓库整体添加，否则可能再次只保存 gitlink。
