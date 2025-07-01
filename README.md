# floderbackup
锐意制作中...

---

# Safe Move with Symlink (安全搬移并创建符号链接)

这是一个功能强大的 PowerShell 脚本，用于将 Windows 系统中的目录（特别是位于C盘的用户配置、缓存或数据目录）安全地迁移到另一个驱动器，并在原始位置创建一个符号链接（目录联接）。这可以有效释放系统盘空间，同时保持应用程序的正常运行。

脚本经过了多次迭代，集成了备份、回滚、路径镜像等多种安全机制，以确保数据在迁移过程中的完整性和操作的健壮性。

## ✨ 功能特性

- **镜像式迁移**: 自动在目标位置创建能反映原始路径的目录结构，从根本上解决了同名目录（如 `cache`）的冲突问题。
  - 例如，`C:\Users\YourName\.cache` -> `E:\CdiskMove\C_drive\Users\YourName\.cache`
- **全自动备份与还原**:
  - 在执行任何操作前，自动将源目录完整备份。
  - 维护一个 `backup_map.json` 文件，记录备份与原始路径的映射关系。
  - 支持 `--restore` 参数，可以根据备份名轻松地将数据还原到原始位置。
- **原子化操作与回滚**: 采用“备份 -> 复制 -> 强制删除 -> 创建链接”的清晰步骤。如果在任何关键步骤（如删除源目录）失败，脚本会自动尝试回滚，删除已复制的数据，以保持系统状态的一致性。
- **健壮的错误处理**:
  - 启动时自动检查管理员权限。
  - 检查源目录是否已为符号链接，防止重复操作。
  - 检查目标路径是否已存在，避免意外覆盖数据。
  - 对文件/目录占用导致删除失败等情况有明确的提示和处理。
- **清晰的日志输出**: 每一步操作都有详细的控制台输出，让您清楚地了解脚本的执行过程。

## 🚀 快速开始

### 依赖
- Windows 操作系统
- PowerShell (建议 5.1 或更高版本)

### 安装
1. 下载 `safe_move_with_symlink.ps1` 脚本文件到你的电脑，例如放在 `E:\Scripts` 目录下。
2. 为了能够在任何地方方便地运行，你可以将该目录添加到系统的 `Path` 环境变量中。

### 使用方法

**重要提示**: 必须 **以管理员身份运行 PowerShell** 来执行此脚本，因为创建符号链接需要管理员权限。

#### 1. 移动目录

这是最常用的功能。它会将一个目录移动到指定的目标根目录下，并在原地创建链接。

**语法:**
```powershell
.\safe_move_with_symlink.ps1 <源目录路径> <目标根目录>
```

**示例:**

假设你想将 Docker Desktop 的数据目录从C盘移动到E盘的 `E:\CdiskMove` 文件夹下。

```powershell
# 打开一个管理员PowerShell窗口
PS> .\safe_move_with_symlink.ps1 "C:\Users\YourName\AppData\Local\Docker" "E:\CdiskMove"
```

**执行流程:**
1.  **备份**: `C:\Users\YourName\AppData\Local\Docker` -> `E:\CdiskMove\__bak__\C_Users_YourName_AppData_Local_Docker.bak`
2.  **复制**: `C:\Users\YourName\AppData\Local\Docker` -> `E:\CdiskMove\C_drive\Users\YourName\AppData\Local\Docker`
3.  **删除**: 成功复制后，强制删除 `C:\Users\YourName\AppData\Local\Docker`。
4.  **链接**: 在 `C:\Users\YourName\AppData\Local` 目录下创建一个名为 `Docker` 的符号链接，指向新的E盘位置。

#### 2. 还原目录

如果你想撤销之前的操作，或者需要从备份中恢复数据，可以使用还原功能。

**语法:**
```powershell
.\safe_move_with_symlink.ps1 --restore <备份名称>
```

`<备份名称>` 是存放在 `__bak__` 目录下的 `.bak` 文件夹的名称。你可以在 `__bak__\backup_map.json` 文件中找到所有备份名称和其对应的原始路径。

**示例:**

从之前的备份中还原 Docker 数据。

```powershell
# 打开一个管理员PowerShell窗口
PS> .\safe_move_with_symlink.ps1 --restore "C_Users_YourName_AppData_Local_Docker.bak"
```

**执行流程:**
1.  脚本会查找 `backup_map.json` 来确定原始路径。
2.  如果原始路径上存在符号链接，会先将其删除。
3.  将备份目录的内容复制回原始路径。

## ⚠️ 注意事项

- **关闭相关程序**: 在执行移动操作前，请尽量关闭正在使用源目录的应用程序。虽然脚本会尝试强制删除，但关闭程序能大大提高成功率。
- **备份目录**: 脚本会在目标根目录同级的 `__bak__` 文件夹（硬编码为 `E:\CdiskMove\__bak__`）中创建备份。请确保该位置有足够的空间，并定期检查是否需要清理旧的备份。
- **符号链接 vs. 目录联接**: 脚本内部使用 `mklink /J` 创建**目录联接 (Directory Junction)**，它对于重定向本地目录有最好的兼容性。在PowerShell中，它和符号链接（Symbolic Link）都显示为 `LinkType`。

## 贡献

欢迎通过 Issues 和 Pull Requests 来改进这个脚本。如果你发现了任何bug或者有新的功能建议，请不要犹豫，分享你的想法！

---

_这个脚本是与AI助手共同协作、经过多次迭代和真实场景调试的成果，旨在提供一个真正可靠的Windows目录迁移解决方案。_
