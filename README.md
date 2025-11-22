# adjustBias
this repository is using to adjust bias for tiangong2.0

## 项目概述

adjustBias 是一个用于调整 Tiangong2.0 机器人运动控制参数的桌面工具（基于 Qt）。程序通过 SSH 连接到目标设备，读取/写入远端的参数文件（默认路径: `/home/ubuntu/data/param/rl_control_new.txt`），并在 UI 中提供便捷的参数调整和保存功能。

## 依赖（开发/运行）

- Qt 5 或 Qt 6（项目中使用了对 Qt6 的兼容判断，Qt5/Qt6 均可编译，但建议使用与你的系统配套的 Qt 版本）
- libssh2（用于 SSH 连接）
- Windows: 需要 Winsock（ws2_32），并确保 libssh2 的动态库可用（`libssh2.dll`）
- CMake, Ninja/Make（构建系统）

## 构建（在 Windows 上示例）

1. 安装依赖（Qt、libssh2、CMake 等）。
2. 创建构建目录并运行 CMake：

```powershell
mkdir build; cd build
cmake -G Ninja ..
ninja
```

你也可以使用 Qt Creator 打开顶级 `CMakeLists.txt` 并直接构建。

## 运行

构建完成后，运行生成的可执行文件（位于 `build/` 或 CMake 指定的输出目录）。在程序界面中：

1. 在 IP 输入框输入目标设备 IP，然后点击“加载”。
2. 成功加载后，UI 会显示当前参数，修改后点击“保存”将通过 SSH 写回远端文件。

## 日志与故障排查

- 程序会在运行目录下生成 `logs/` 文件夹（如果发生异常，会在此输出带时间戳的 `exception_*.log`），用于查看详细错误信息。
- 若加载/保存失败：请先使用宿主机 SSH 客户端确认能否用相同的用户名/密码登录目标设备，检查防火墙与端口连通性。你可以使用 PowerShell 的 `Test-NetConnection -ComputerName <IP> -Port 22` 来测试端口。

## 注意

- 仓库已添加 `.gitignore`，会忽略 `build/`、IDE 配置和运行产生的日志等，避免把构建产物提交到版本库。


--------


## 更新 (Changelog)

2025-11-22 — 修复与增强

- 改进远程配置读取的可靠性
	- 文件: `ConfigReader.hpp`
	- 描述: 将 `executeRemoteCommand` 的读取逻辑改为阻塞读取直到 SSH 通道 EOF，避免因非阻塞短超时导致读取到空或不完整的远端配置内容。

- 在加载失败时显示更详细的错误信息
	- 文件: `widget.cpp`, `widget.h`
	- 描述: 当 `main_load()` 捕获到 SSH 或其它异常时，会把详细错误信息保存在 `Widget::lastErrorMessage` 并在 UI 的“加载失败”对话框中显示，方便排查（例如认证失败、网络不可达、libssh2 错误等）。

- 已将上述改动提交至远程仓库（排除了 `build/` 目录）
	- 提交信息: `修复：改进远端配置读取并显示加载时详细错误信息`
	- 提交分支: `main`

如何验证 / 测试

1. 在程序中点击“加载”（Load）按钮：
	 - 成功时会正常读取并将远端配置显示到 UI；
	 - 失败时弹窗会包含具体错误信息（例如 `SSH连接异常: ...`），并在项目目录下 `logs/` 输出异常日志文件（`exception_*.log`）。

2. 若遇到错误，建议按下列步骤排查：
	 - 在宿主机上执行网络检测（PowerShell）：
		 ```powershell
		 Test-NetConnection -ComputerName <目标IP> -Port 22
		 ```
	 - 尝试使用系统/其它 SSH 客户端手动登录以确认用户名/密码与网络是否可达：
		 ```powershell
		 ssh ubuntu@<目标IP>
		 ```
	 - 查看 `logs/` 目录下最新的 `exception_*.log`，或把 UI 弹窗中的详细错误信息贴出来以便进一步诊断。



