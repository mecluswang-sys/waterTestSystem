# Windows系统代码页设置为UTF-8的方法

## 方法1：图形界面设置（Windows 10/11）

1. 打开"设置" → "时间和语言" → "语言和区域"
2. 点击"管理语言设置"
3. 在"区域"窗口中，点击"管理"选项卡
4. 点击"更改系统区域设置"
5. 勾选"Beta版：使用 Unicode UTF-8 提供全球语言支持"
6. 点击"确定"
7. 重启计算机

## 方法2：通过注册表（需要管理员权限）

运行以下PowerShell命令（以管理员身份）：

```powershell
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Nls\CodePage" -Name "ACP" -Value "65001"
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Nls\CodePage" -Name "OEMCP" -Value "65001"
```

然后重启计算机。

## 方法3：临时设置（仅当前终端会话）

```powershell
chcp 65001
```

## 注意事项

⚠️ **不推荐修改系统代码页**，原因如下：

1. **可能导致兼容性问题**：某些老旧的中文软件可能无法正常显示
2. **需要重启系统**：修改才能生效
3. **影响范围广**：会影响所有未明确指定编码的程序

✅ **我们的项目已经通过 `/utf-8` 编译选项完美解决了编码问题，无需修改系统设置！**

## 当前项目的编码配置（已完成）

```cmake
# CMakeLists.txt 中已包含
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
```

这个设置：

- ✅ 只影响我们的项目
- ✅ 不需要管理员权限
- ✅ 不需要重启系统
- ✅ 不影响其他程序
- ✅ 完全支持中文注释和字符串

**建议：保持当前配置即可，不要修改系统代码页。**
