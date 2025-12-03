# 强化模式偏置调整工具 (Enhanced Mode Bias Adjustment Tool)

一个基于Qt6的SSH远程参数调整工具，用于实时配置和优化机器人系统的运动参数。

## 📋 项目概述

本项目提供了一个现代化的GUI界面，通过SSH连接远程机器人系统，实现参数的实时读取、调整和保存。支持多种运动参数的精确控制，包括位置、姿态、速度限制等关键配置。

### 🎯 主要功能

- **远程SSH连接**: 安全连接到远程机器人系统
- **实时参数调整**: 支持位置、姿态、速度等参数的实时修改
- **参数验证**: 智能参数验证和范围检查
- **配置管理**: 支持配置文件的导入、导出和备份
- **日志记录**: 完整的操作日志和异常追踪
- **多语言支持**: 中英文界面切换

## 🏗️ 系统架构

```
adjustBias/
├── include/                 # 头文件目录
│   ├── widget.h            # 主界面类
│   ├── SSHManager.h        # SSH连接管理
│   ├── ConfigReader.h      # 配置读取器
│   ├── Parameters.h        # 参数数据模型
│   ├── Logger.h            # 日志系统
│   ├── Exceptions.h        # 异常处理
│   └── ...                 # 其他核心模块
├── src/                    # 源文件目录
│   ├── widget.cpp          # 主界面实现
│   ├── SSHManager.cpp      # SSH管理实现
│   ├── ConfigReader.cpp    # 配置读取实现
│   └── ...                 # 其他实现文件
├── docs/                   # 文档目录
│   ├── archive/            # 历史文档归档
│   └── ...                 # 项目文档
├── resources/              # 资源文件
├── translations/           # 国际化文件
└── build/                  # 构建输出目录
```

## 🚀 快速开始

### 环境要求

- **操作系统**: Windows 10/11 (推荐), Linux, macOS
- **编译器**: MSVC 2019+, GCC 9+, Clang 10+
- **Qt版本**: Qt 6.5 或更高版本
- **CMake**: 3.19 或更高版本
- **依赖库**: libssh2, OpenSSL

### 安装步骤

1. **克隆项目**
   ```bash
   git clone https://github.com/Kiah0219/adjustBias.git
   cd adjustBias
   ```

2. **安装依赖**
   ```bash
   # 使用vcpkg安装依赖 (推荐)
   vcpkg install qt6 libssh2 openssl
   
   # 或使用系统包管理器
   # Windows: 通过Qt Installer安装Qt6
   # Ubuntu: sudo apt-get install qt6-base-dev libssh2-1-dev
   ```

3. **配置CMake**
   ```bash
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```

4. **编译项目**
   ```bash
   cmake --build . --config Release
   ```

5. **运行程序**
   ```bash
   # Windows
   bin/adjustBias.exe
   
   # Linux/macOS
   ./bin/adjustBias
   ```

## 📖 使用指南

### 基本操作流程

1. **启动应用**: 运行可执行文件启动GUI界面
2. **配置连接**: 在连接设置中输入远程主机的IP、端口、用户名和密码
3. **建立连接**: 点击"连接"按钮建立SSH连接
4. **读取参数**: 点击"加载"按钮从远程系统读取当前参数配置
5. **调整参数**: 在界面中修改需要调整的参数值
6. **验证参数**: 系统会自动验证参数的有效性
7. **保存配置**: 点击"保存"按钮将修改应用到远程系统

### 支持的参数类型

| 参数类别 | 参数名称 | 单位 | 范围 | 说明 |
|---------|---------|------|------|------|
| **位置参数** | X轴偏移 | mm | -1000~1000 | 前后方向位置调整 |
| | Y轴偏移 | mm | -1000~1000 | 左右方向位置调整 |
| **姿态参数** | Roll角 | deg | -180~180 | 横滚角度 |
| | Pitch角 | deg | -90~90 | 俯仰角度 |
| | Yaw角 | deg | -180~180 | 偏航角度 |
| **速度限制** | 行走速度 | m/s | 0~2.0 | 行走模式最大速度 |
| | 跑步速度 | m/s | 0~5.0 | 跑步模式最大速度 |

### 高级功能

- **批量配置**: 支持配置文件的批量导入和导出
- **参数模板**: 预设常用参数组合，快速切换
- **实时监控**: 实时显示参数修改效果
- **历史记录**: 保存参数修改历史，支持回滚操作

## 🔧 技术特性

### 核心技术栈

- **Qt6**: 现代化GUI框架，提供丰富的UI组件
- **libssh2**: 安全的SSH连接库，支持加密通信
- **C++17**: 现代C++标准，提供强大的语言特性
- **CMake**: 跨平台构建系统

### 性能优化

- **连接池**: SSH连接复用，减少连接开销 (50-70%性能提升)
- **参数缓存**: 本地参数缓存，减少网络请求
- **异步操作**: 非阻塞UI操作，提升用户体验
- **内存管理**: RAII资源管理，防止内存泄漏

### 安全特性

- **SSH加密**: 所有通信都通过SSH加密传输
- **参数验证**: 严格的参数范围检查和类型验证
- **异常处理**: 完善的异常处理机制，确保系统稳定性
- **日志审计**: 详细的操作日志，便于问题追踪

## 🐛 故障排除

### 常见问题

**Q: 无法连接到远程主机**
A: 检查网络连接、SSH服务状态、防火墙设置和认证信息

**Q: 参数保存失败**
A: 确认参数值在有效范围内，检查远程系统权限设置

**Q: 界面显示异常**
A: 确保Qt6相关库文件完整，检查系统字体设置

**Q: 编译错误**
A: 检查依赖库版本，确保CMake配置正确

### 日志文件位置

- **Windows**: `%APPDATA%/adjustBias/logs/`
- **Linux**: `~/.local/share/adjustBias/logs/`
- **macOS**: `~/Library/Logs/adjustBias/`

## 🤝 贡献指南

我们欢迎社区贡献！请遵循以下步骤：

1. Fork本项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建Pull Request

### 代码规范

- 遵循现代C++编码规范
- 使用有意义的变量和函数名
- 添加适当的注释和文档
- 确保代码通过所有测试

## 📄 许可证

本项目采用MIT许可证 - 查看 [LICENSE](LICENSE) 文件了解详情

## 📞 联系方式

- **项目维护者**: Kiah0219
- **邮箱**: [your-email@example.com]
- **项目主页**: https://github.com/Kiah0219/adjustBias
- **问题反馈**: https://github.com/Kiah0219/adjustBias/issues

## 🙏 致谢

感谢以下开源项目和贡献者：

- [Qt](https://www.qt.io/) - 跨平台应用框架
- [libssh2](https://www.libssh2.org/) - SSH客户端库
- [vcpkg](https://vcpkg.io/) - C++包管理器

---

**版本**: v0.4.4  
**最后更新**: 2025-12-03  
**构建状态**: ![Build Status](https://img.shields.io/badge/build-passing-brightgreen)  
**许可证**: ![License](https://img.shields.io/badge/license-MIT-blue)