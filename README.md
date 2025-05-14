# 文件占用检查器 (File In Use Checker)

一个简单的 Windows 工具，用于检查文件是否被其他进程占用，并显示具体是哪些进程在使用该文件。

## 功能特点

- 检查指定文件是否被其他进程占用
- 显示占用文件的进程信息，包括：
  - 进程名称
  - 进程 ID
  - 进程完整路径
- 支持检查文件夹内所有文件的占用情况
- 支持两种便捷的使用方式：
  - 拖放文件到程序图标上
  - 通过右键菜单选择文件

## 使用方法

1. **拖放方式**：
   - 直接将要检查的文件拖到程序图标上即可
   - 程序会立即显示文件的占用情况

2. **右键菜单方式**：
   - 在要检查的文件上右键
   - 选择"发送到"
   - 点击"FileInUseChecker"

## 编译说明

使用 g++ 编译（需要 MinGW-w64）：

```bash
g++ FileInUseChecker.cpp -o FileInUseChecker.exe -mwindows -lrstrtmgr -lpsapi
```

编译选项说明：
- `-mwindows`: 创建 Windows GUI 应用程序
- `-lrstrtmgr`: 链接重启管理器库
- `-lpsapi`: 链接进程状态 API 库

## 系统要求

- Windows 7 及以上操作系统
- 需要管理员权限以获取完整的进程信息

## 📥 安装步骤

1. 下载最新版本的发布包
2. 将 `FileInUseChecker.exe` 复制到指定目录（如：`C:\Program Files\FileInUseChecker\`）
3. 双击运行 `导入右键菜单.reg` 添加右键菜单项
4. 在确认对话框中点击"是"

## 🎯 使用方法

1. 在 Windows 资源管理器中右键点击任意文件或文件夹
2. 在右键菜单中选择"查看文件占用"
3. 程序将显示正在使用该文件/文件夹的所有进程信息，包括：
   - 进程名称
   - 进程ID (PID)
   - 进程完整路径

## 🗑️ 卸载方法

1. 删除程序文件夹（如：`C:\Program Files\FileInUseChecker\`）
2. 删除以下注册表项：
   ```
   HKEY_CLASSES_ROOT\*\shell\FileInUseCheck
   HKEY_CLASSES_ROOT\Directory\shell\FileInUseCheck
   HKEY_CLASSES_ROOT\Directory\Background\shell\FileInUseCheck
   ```

## 🔧 技术说明

- 使用 C++ 开发
- 采用 Windows Restart Manager API 检测文件占用
- 支持 NT 路径和 DOS 路径的自动转换
- 实现了文件路径的智能处理（支持带空格和特殊字符的路径）
- 使用 RAII 技术确保资源正确释放
- 支持 Unicode 字符集，完整支持中文路径

## 📝 注意事项

1. 首次使用时需要以管理员身份运行
2. 某些系统进程的文件占用可能需要管理员权限才能检测
3. 如果检测不到文件占用，请尝试以管理员身份运行程序

## 🤝 贡献

欢迎提交 Issue 和 Pull Request 来帮助改进这个项目。

## 📄 许可证

[MIT License](LICENSE)

## 📞 联系方式

如有问题或建议，请通过以下方式联系：
- 在 GitHub 上提交 Issue
- 发送邮件至：[your-email@example.com] 