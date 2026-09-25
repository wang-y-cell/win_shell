# win_shell

在 Windows 上用 C++ 实现的一组 Linux 风格命令行工具。每个命令是独立的 `.exe`，放进 `PATH` 后即可当 `ls`、`cat`、`grep` 等使用。PowerShell 脚本负责编码、提示符，以及去掉会挡住这些命令的别名。

## 环境

- Windows
- CMake 3.16+
- C++17 编译器（MinGW 或 MSVC）
- PowerShell 5+ / PowerShell 7

## 编译

在仓库根目录：

```powershell
cmake -S . -B build
cmake --build build
# 将生成的bin/目录添加到PATH环境变量中
```

可执行文件输出到 `bin\`。某个命令的参数说明：

```powershell
.\bin\ls.exe --help
```

## 装进当前终端

`load.ps1` 只影响**当前窗口**：UTF-8、自定义提示符、把 `bin` 加到本次 `PATH`、删除冲突别名。

```powershell
.\powershell\load.ps1
```

也可以点源：`. .\powershell\load.ps1`。关掉终端后失效。

提示符示例：

```text
windows@PS:F:/.../win_shell$
```

## 写入启动配置

`build.ps1` 在 `$PROFILE` 里写入一段标记块，新开的 PowerShell 会自动执行 `load.ps1`，并立刻对当前窗口再 `load` 一次。

```powershell
.\powershell\build.ps1
```

仓库换了位置时再跑一次即可更新 `$PROFILE` 里的路径。

`remove.ps1` 只删掉这段标记，其它配置不动：

```powershell
.\powershell\remove.ps1
```

当前窗口不会因此恢复默认提示符；新终端不再自动加载。

## PowerShell 会挡住的命令

PowerShell 自带 `ls`、`cat`、`pwd` 等别名，指向 `Get-ChildItem` / `Get-Content`。`load` 会去掉这些别名，否则敲 `ls` 仍是系统那一套。

`cd` **不能**做成 `.exe`：子进程里改目录，退回当前 shell 不会变。继续用 PowerShell 的 `cd` / `Set-Location`。

## 已有命令

| 类别 | 命令 |
| --- | --- |
| 列表 / 路径 | `ls` `ll` `pwd` `tree` `which` `basename` `dirname` |
| 文本 | `cat` `head` `tail` `wc` `tee` `sort` `uniq` `grep` `diff` |
| 文件 | `mkdir` `touch` `rm` `cp` `mv` `ln` |
| 磁盘 / 查找 | `du` `df` `find` |
| 其它 | `clear` |

`ll` 等同 `ls -alh`。各命令支持 `--help`。

`ls` 已实现常用 GNU 风格参数（`-a` `-l` `-h` `-R` `-F` `--color` `--sort` 等）。不做与 Windows 无关的项：`-G`/`-g`/`-o`、`--author`、`-Z`、`--hyperlink`、版本排序 `-v` 等。

## 目录

```text
bin/            编译产物
include/utils/  公共头文件（解析、颜色、输出等）
src/            各命令与公共库
powershell/     load / build / remove 与提示符
```

## 说明

- 不会修改用户级系统 `PATH`，只改当前会话。
- `load` 不会编译；改了 C++ 源码后需要重新 `cmake --build`。
- 执行策略过严时：`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`。
