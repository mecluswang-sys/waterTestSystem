# Water Test System - 构建脚本说明

本目录包含项目的构建和部署脚本。

## 脚本列表

### 1. setup-cache.ps1

设置vcpkg二进制缓存，避免重复编译Qt等大型库。

**使用方法：**

```powershell
.\scripts\setup-cache.ps1
```

**功能：**

- 创建缓存目录 `~\.vcpkg\bincache`
- 设置环境变量 `VCPKG_BINARY_SOURCES`
- 永久保存配置（需要时重启终端生效）

**只需运行一次！** 之后vcpkg会自动使用缓存。

---

### 2. build.ps1

完整构建项目（首次使用或修改CMakeLists.txt后）。

**使用方法：**

```powershell
# 编译Release版本（默认）
.\scripts\build.ps1

# 编译Debug版本
.\scripts\build.ps1 -BuildType Debug
```

**功能：**

- 配置CMake
- 安装vcpkg依赖（使用缓存，不会重复编译Qt）
- 编译整个项目

**输出：** `build/bin/Release/WaterTestSystem.exe`

---

### 3. rebuild.ps1

快速重编译（日常开发使用）。

**使用方法：**

```powershell
# 快速编译Release版本
.\scripts\rebuild.ps1

# 快速编译Debug版本
.\scripts\rebuild.ps1 -BuildType Debug
```

**功能：**

- 仅编译修改过的源文件
- 不重新配置CMake
- 速度最快（几秒钟）

**适用场景：** 修改.cpp/.h文件后重新编译

---

### 4. clean.ps1

清理编译产物（保留Qt等依赖库）。

**使用方法：**

```powershell
.\scripts\clean.ps1
```

**功能：**

- 删除编译的目标文件
- 删除CMake缓存
- **保留** `build/vcpkg_installed` 目录（Qt在这里）

**用途：** 遇到编译问题时清理重建

---

### 5. deploy.ps1

打包程序到deploy目录，可直接复制到目标机器。

**使用方法：**

```powershell
.\scripts\deploy.ps1
```

**功能：**

- 复制可执行文件和所有DLL
- 复制Qt平台插件
- 创建配置文件和日志目录
- 生成启动脚本和使用说明

**输出：** `deploy/` 目录，包含完整的可部署程序

---

## 典型工作流程

### 首次使用

```powershell
# 1. 设置vcpkg缓存（只需一次）
.\scripts\setup-cache.ps1

# 2. 完整编译项目
.\scripts\build.ps1

# 3. 打包部署
.\scripts\deploy.ps1
```

### 日常开发

```powershell
# 修改代码后快速重编译
.\scripts\rebuild.ps1

# 测试
.\build\bin\Release\WaterTestSystem.exe

# 部署到deploy目录
.\scripts\deploy.ps1
```

### 遇到编译问题

```powershell
# 清理后重建
.\scripts\clean.ps1
.\scripts\build.ps1
```

### 完全重建（很少需要）

```powershell
# 删除整个build目录
Remove-Item build -Recurse -Force

# 重新编译（会使用vcpkg缓存，不会重新编译Qt）
.\scripts\build.ps1
```

---

## 编译时间参考

| 操作 | 时间 | 说明 |
|------|------|------|
| setup-cache.ps1 | < 1秒 | 只需运行一次 |
| build.ps1（首次） | ~30分钟 | 编译Qt等依赖库 |
| build.ps1（有缓存） | ~5分钟 | 从缓存安装Qt |
| rebuild.ps1 | 5-30秒 | 只编译修改的文件 |
| deploy.ps1 | < 10秒 | 复制文件 |

---

## 故障排除

### Q: 每次都重新编译Qt？

A: 运行 `.\scripts\setup-cache.ps1` 启用vcpkg缓存

### Q: 编译报错找不到Qt？

A: 运行 `.\scripts\build.ps1` 完整重建

### Q: 修改代码后编译很慢？

A: 使用 `.\scripts\rebuild.ps1` 而不是 `build.ps1`

### Q: 部署的程序无法运行？

A: 检查是否缺少platforms/qwindows.dll插件
