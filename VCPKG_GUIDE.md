# 使用vcpkg管理Snap7依赖

本文档说明如何使用vcpkg来管理Snap7库依赖。

## 方法1：使用vcpkg（推荐）

### Windows

#### 1. 安装vcpkg

```powershell
# 克隆vcpkg仓库
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# 引导安装
.\bootstrap-vcpkg.bat

# 集成到系统（可选，需要管理员权限）
.\vcpkg integrate install
```

#### 2. 设置环境变量

```powershell
# 设置VCPKG_ROOT环境变量
setx VCPKG_ROOT "C:\vcpkg"

# 或添加到系统环境变量
# 控制面板 -> 系统 -> 高级系统设置 -> 环境变量
# 新建系统变量：VCPKG_ROOT = C:\vcpkg
```

#### 3. 安装Snap7

```powershell
cd C:\vcpkg

# 安装snap7 (x64)
.\vcpkg install snap7:x64-windows

# 或安装x86版本
.\vcpkg install snap7:x86-windows

# 查看已安装的包
.\vcpkg list
```

#### 4. 构建项目

```powershell
cd C:\Users\mickey\workspace\4_cpp\1-huanjiang

# 直接运行构建脚本（会自动检测vcpkg）
.\build.bat

# 或手动指定vcpkg toolchain
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Linux

#### 1. 安装vcpkg

```bash
# 克隆vcpkg仓库
cd ~
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# 引导安装
./bootstrap-vcpkg.sh

# 集成到系统（可选）
./vcpkg integrate install
```

#### 2. 设置环境变量

```bash
# 添加到 ~/.bashrc 或 ~/.zshrc
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc
source ~/.bashrc
```

#### 3. 安装Snap7

```bash
cd ~/vcpkg

# 安装snap7
./vcpkg install snap7

# 查看已安装的包
./vcpkg list
```

#### 4. 构建项目

```bash
cd ~/workspace/4_cpp/1-huanjiang

# 直接运行构建脚本
./build.sh

# 或手动指定vcpkg toolchain
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
make -j$(nproc)
```

## 方法2：手动安装Snap7

如果不想使用vcpkg，仍然可以手动安装：

### Windows

1. 下载Snap7：<https://sourceforge.net/projects/snap7/>
2. 解压到 `C:\snap7`
3. 设置环境变量：`SNAP7_ROOT=C:\snap7`
4. 运行 `build.bat`

### Linux

```bash
wget https://sourceforge.net/projects/snap7/files/latest/download -O snap7.zip
unzip snap7.zip
cd snap7-full-*/build/unix
make -f x86_64_linux.mk
sudo make -f x86_64_linux.mk install
```

## vcpkg优势

### ✅ 优点

1. **自动依赖管理**：自动下载、编译和安装
2. **跨平台**：Windows、Linux、macOS统一管理
3. **版本控制**：可以指定特定版本
4. **集成简单**：CMake原生支持
5. **二进制缓存**：可以共享编译好的二进制文件

### ❌ 注意事项

1. 首次安装需要从源码编译，可能需要一些时间
2. 需要联网下载源码
3. 需要额外磁盘空间（vcpkg目录约几GB）

## 故障排除

### 问题1：vcpkg未找到

```powershell
# 检查VCPKG_ROOT是否设置
echo %VCPKG_ROOT%   # Windows
echo $VCPKG_ROOT    # Linux

# 手动指定toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 问题2：Snap7未找到

```powershell
# 确认snap7已安装
vcpkg list | grep snap7

# 重新安装
vcpkg remove snap7
vcpkg install snap7:x64-windows
```

### 问题3：编译错误

```powershell
# 清理并重新构建
rmdir /s /q build
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

## VSCode集成

如果使用VSCode，可以在 `.vscode/settings.json` 中配置：

```json
{
    "cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    }
}
```

## Visual Studio集成

Visual Studio 2019及以上版本内置vcpkg支持：

1. 打开 **工具** -> **选项** -> **CMake**
2. 在 **CMake toolchain file** 中设置：

   ```
   C:\vcpkg\scripts\buildsystems\vcpkg.cmake
   ```

## 检查安装

运行以下命令验证安装：

```powershell
# 检查vcpkg
vcpkg version

# 检查snap7
vcpkg list snap7

# 测试编译
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

## 推荐配置

对于团队开发，推荐在项目根目录创建 `vcpkg.json` manifest文件：

```json
{
    "name": "water-test-system",
    "version": "1.0.0",
    "dependencies": [
        "snap7"
    ]
}
```

然后使用manifest模式：

```powershell
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
```

vcpkg会自动安装manifest中列出的所有依赖！
