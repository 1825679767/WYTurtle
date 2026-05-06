# WYTurtle

WYTurtle 是一份基于 Turtle/MaNGOS 1.12 服务端源码整理的工程，目标客户端版本为 `CLIENT_BUILD_1_12_1`。

本项目主要面向想在 Windows 环境下自行编译、部署和二次开发 Turtle 1.12 服务端的使用者。本仓库提供服务端源码、SQL 和必要文档；编译器、数据库、ACE、运行目录、构建目录和本地生成文件由使用者自行准备。

当前工程默认启用 Lua 脚本兼容层，用来在 Turtle 1.12 核心上运行部分从 3.3.5 Eluna 迁移过来的自定义 Lua 功能。Lua 移植不是照搬 AzerothCore 3.3.5，而是按 Turtle 1.12 的真实系统做适配。没有真实核心入口的 3.3.5 专属功能不会做假接口。

Lua 移植状态请看：

```text
docs/lua-port-status.md
```

## 目录说明

```text
src/                 服务端源码，包含 realmd、mangosd、game、scripts 等模块
dep/                 第三方依赖源码或依赖占位目录
sql/                 数据库结构、world 数据和更新 SQL
docs/                项目文档和 Lua 移植状态记录
cmake/               CMake 查找模块和构建辅助脚本
```

## Windows 编译环境

推荐环境：

```text
Windows 10 / Windows 11 x64
Visual Studio 2022，安装“使用 C++ 的桌面开发”
CMake 3.16 或更新版本
Git for Windows
MySQL Server 5.7，用于运行服务端数据库
ACE，用于编译服务端
```

默认构建选项：

```text
USE_LUA=ON          启用 Lua 脚本兼容层
USE_SCRIPTS=ON      编译 C++ 脚本模块
USE_STD_MALLOC=ON   使用标准 malloc，通常不需要 TBB
```

## 第三方依赖

### ACE

本项目需要 ACE。使用者可以自行选择安装目录，例如：

```text
C:\deps\ACE_wrappers
D:\deps\ACE_wrappers
```

后续 CMake 使用 `-DACE_ROOT=<ACE_ROOT>` 指向该目录。

准备步骤：

1. 下载 ACE 源码包并解压到自选目录。
2. 使用 Visual Studio 打开 ACE 自带的解决方案，例如 `ACE_vs2019.sln` 或 `ACE_wrappers_vs2019.sln`。VS2022 可以升级打开。
3. 选择 `x64` 和 `Release`，编译 ACE。
4. 确认生成了类似下面的文件：

```text
<ACE_ROOT>\ace
<ACE_ROOT>\lib\ACE.lib
<ACE_ROOT>\lib\ACE.dll
```

### MySQL / OpenSSL / Zlib 编译依赖

Windows 下当前 CMake 会查找仓库内的依赖目录：

```text
dep\windows\include\mysql
dep\windows\include\openssl
dep\windows\include\zlib
dep\windows\lib\x64_release
dep\windows\lib\x64_Debug
```

如果 GitHub 仓库不上传第三方二进制库，使用者需要自行准备这些头文件和 `.lib` / `.dll`，并放到上面的结构中，或自行调整 CMake 配置。

Release 目录通常需要包含：

```text
libmysql.lib
libmySQL.dll
libssl.lib
libcrypto.lib
```

Debug 编译时需要对应的 Debug 版本库。建议初次编译只使用 `Release`。

MySQL Server 5.7 是运行数据库服务用的版本；编译阶段用到的是 MySQL 客户端开发头文件和链接库。

## 生成 Visual Studio 工程

以下命令中的路径请按自己的机器修改：

```text
<source_dir>   源码目录，例如 D:\Git\WYTurtle
<build_dir>    构建目录，例如 D:\Build\WYTurtle
<install_dir>  安装输出目录，例如 D:\Server\WYTurtle
<ACE_ROOT>     ACE 根目录，例如 D:\deps\ACE_wrappers
```

在 PowerShell 或 “Developer PowerShell for VS 2022” 中执行：

```powershell
cmake -S <source_dir> `
      -B <build_dir> `
      -G "Visual Studio 17 2022" `
      -A x64 `
      -DPREFIX=<install_dir> `
      -DACE_ROOT=<ACE_ROOT> `
      -DUSE_LUA=ON `
      -DUSE_SCRIPTS=ON
```

示例：

```powershell
cmake -S D:\Git\WYTurtle `
      -B D:\Build\WYTurtle `
      -G "Visual Studio 17 2022" `
      -A x64 `
      -DPREFIX=D:/Server/WYTurtle `
      -DACE_ROOT=D:/deps/ACE_wrappers `
      -DUSE_LUA=ON `
      -DUSE_SCRIPTS=ON
```

注意：

- 本项目要求 out-of-source build，不要在源码根目录直接生成 `CMakeCache.txt`。
- `-A x64` 要和你准备的第三方库架构一致。
- `-DPREFIX` 是编译安装后的服务端输出目录。

## 编译和安装

完整编译并安装：

```powershell
cmake --build <build_dir> --config Release --target INSTALL
```

只编译世界服：

```powershell
cmake --build <build_dir> --config Release --target mangosd -- /m
```

只编译登录服：

```powershell
cmake --build <build_dir> --config Release --target realmd -- /m
```

只编译 Lua 语法检查工具：

```powershell
cmake --build <build_dir> --config Release --target lua_compiler -- /m
```

安装完成后，`<install_dir>` 中应包含：

```text
realmd.exe
mangosd.exe
lua52_compiler.exe
*.conf 或 *.conf.dist
运行所需 DLL
```

检查 Lua 脚本语法：

```powershell
<install_dir>\lua52_compiler.exe -p <script_file.lua>
```

## GitHub Actions 发布

仓库内提供了 Windows 自动编译和 Release 发布工作流：

```text
.github/workflows/windows-release.yml
```

使用方式：

1. 在 GitHub 仓库页面打开 `Actions`。
2. 选择 `Windows Release`。
3. 点击 `Run workflow`。
4. 填写 `release_tag`，例如 `v1.0.0`。如果留空，会使用 `manual-运行编号`。
5. 工作流会自动准备 ACE、MySQL 5.7 客户端库和 OpenSSL，编译 `Release`，生成压缩包并发布到 `Releases`。

也可以推送 `v*` 标签触发发布：

```powershell
git tag v1.0.0
git push origin v1.0.0
```

## 数据库准备

运行服务端前需要准备 MySQL 5.7，并导入对应数据库结构和数据。

常见 SQL 文件位置：

```text
sql\_structure_logon.sql
sql\_structure_characters.sql
sql\_structure_logs.sql
sql\database_updates\*.sql
```

world 基础数据库可能因为体积或授权原因不随源码仓库上传。使用者需要自行准备与该核心匹配的 Turtle/MaNGOS 1.12 world 数据库；如果仓库或发布页另外提供了 world 数据包，再按对应说明导入。

具体数据库名、账号、密码和端口以 `realmd.conf`、`mangosd.conf` 中的连接字符串为准。

基本流程：

1. 安装并启动 MySQL 5.7。
2. 创建登录库、角色库、世界库和日志库。
3. 导入结构 SQL。
4. 导入与该核心匹配的 world 基础数据。
5. 按顺序导入 `sql\database_updates` 中需要的更新 SQL。
6. 修改服务端配置文件里的数据库连接信息。

## 启动服务端

先启动登录服：

```powershell
<install_dir>\realmd.exe
```

再启动世界服：

```powershell
<install_dir>\mangosd.exe
```

如果服务端窗口启动后立即退出，优先检查：

```text
数据库连接是否正确
配置文件是否存在
运行目录是否包含所需 DLL
world / characters / realmd 数据库是否完整
端口是否被占用
```

## Lua 脚本

Lua 脚本不随源码仓库提供。需要使用 Lua 时，在运行目录创建脚本目录，并在 `mangosd.conf` 中确认：

```text
Eluna.ScriptPath = "lua_scripts"
```

默认情况下，运行目录中的脚本位置为：

```text
<install_dir>\lua_scripts
```

Lua 移植状态、已支持事件、对象方法、1.12 不适用项和已知差异都记录在：

```text
docs\lua-port-status.md
```

移植原则：

- Turtle 1.12 没有真实载具系统，所以不移植 Vehicle 事件和 Vehicle API。
- 没有真实核心入口的动作接口不做“可注册但不会触发”的假实现。
- 3.3.5 专属系统，例如 Halaa、部分 LFG、公会日历、直接打开邮箱包等，按 Turtle 1.12 的实际能力跳过或另行设计。

## 常见问题

### CMake 提示找不到 ACE

确认 `-DACE_ROOT=<ACE_ROOT>` 指向 ACE 根目录，并且下面内容存在：

```text
<ACE_ROOT>\ace
<ACE_ROOT>\lib\ACE.lib
```

然后重新运行 CMake 配置命令。

### 找不到 MySQL / OpenSSL 库

确认 Windows 依赖目录结构存在：

```text
dep\windows\include\mysql
dep\windows\include\openssl
dep\windows\include\zlib
dep\windows\lib\x64_release
```

如果不使用仓库的 `dep\windows` 结构，需要自行修改 CMake 查找路径或相关变量。

### CMakeCache 配置错了

删除构建目录后重新生成：

```powershell
Remove-Item -LiteralPath <build_dir> -Recurse -Force
cmake -S <source_dir> -B <build_dir> -G "Visual Studio 17 2022" -A x64 -DPREFIX=<install_dir> -DACE_ROOT=<ACE_ROOT> -DUSE_LUA=ON -DUSE_SCRIPTS=ON
```

### Release 和 Debug 库不要混用

Release 编译使用 release 版本第三方库，Debug 编译使用 debug 版本第三方库。初次编译推荐使用：

```text
--config Release
```

### 修改 Lua 后是否必须重编译

普通 Lua 脚本修改不需要重编译 C++。修改运行目录中的 Lua 脚本后，重启 `mangosd` 或使用服务端支持的重载流程即可。

只有修改 `src/game/LuaEngine` 或其他 C++ 源码时才需要重新编译。
