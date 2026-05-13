# WYTurtle
# WY技术交流群：738942437
## 希望我这个项目能抛砖引玉，如果你使用了这个源码，有好的修改的能提交到这个仓库，大家一起将这个仓库做到更好！
本项目面向 Windows 环境下的编译、部署和二次开发。当前工程默认启用 Lua 脚本兼容层，用于在 Turtle 1.12 核心上运行部分从 3.3.5 Eluna 迁移过来的自定义 Lua 功能。

Lua 移植状态记录：

```text
docs\lua-port-status.md
```

## 项目目录

```text
src\                 服务端源码，包含 realmd、mangosd、game、scripts 等模块
dep\                 第三方依赖源码、头文件或依赖占位目录
sql\                 数据库结构、world 数据和更新 SQL
docs\                项目文档和 Lua 移植状态记录
cmake\               CMake 查找模块和构建辅助脚本
CMakeLists.txt       CMake 主配置
README.md            项目说明和 Windows 编译教程
```

## 编译前说明


## 示例目录规划

本文后续命令使用下面的示例目录。实际使用时可以换成自己的目录，例如改到 `C:\WYTurtle` 或其他磁盘；只要把后续命令中的 `D:\WYTurtle` 同步替换即可。建议保持“源码目录”和“编译安装目录”分开，减少构建文件污染源码仓库。

```text
源码目录：              D:\WYTurtle\source
编译和安装目录：        D:\WYTurtle\server
RelWithDebInfo 构建目录：D:\WYTurtle\build\relwithdebinfo
ACE 目录：              D:\WYTurtle\deps\ACE_wrappers
第三方库目录：          D:\WYTurtle\deps\dep-windows-lib
```

源码目录只存放源码和项目文件。不要在 `D:\WYTurtle\source` 中生成 `build`、`bin`、`install` 或日志文件。

## Windows 环境要求

推荐环境：

```text
Windows 10 / Windows 11 x64
Visual Studio 2022
CMake 3.16 或更新版本，推荐 3.27+
Git for Windows
MySQL Server 5.7 x64
ACE
```

默认构建选项：

```text
USE_LUA=ON          启用 Lua 脚本兼容层
USE_SCRIPTS=ON      编译 C++ 脚本模块
USE_STD_MALLOC=ON   使用标准 malloc，通常不需要 TBB
```

推荐构建模式：

```text
RelWithDebInfo
```

`RelWithDebInfo` 性能接近 Release，同时会生成 PDB 调试符号，适合部署测试和排查崩溃。

## 安装 Git

下载并安装 Git for Windows。

安装过程中，`Adjusting your PATH environment` 这一步建议选择：

```text
Git from the command line and also from 3rd-party software
```

安装后打开 PowerShell，执行：

```powershell
git --version
```

能看到版本号即可。

## 安装 Visual Studio 2022

安装 Visual Studio 2022 Community、Professional、Enterprise 或 Build Tools 均可。

安装器中必须勾选：

```text
使用 C++ 的桌面开发
```

建议确认包含以下组件：

```text
MSVC v143 VS 2022 C++ x64/x86 build tools
Windows 10 SDK 或 Windows 11 SDK
C++ CMake tools for Windows
```

安装完成后，开始菜单中应能找到：

```text
x64 Native Tools Command Prompt for VS 2022
Developer PowerShell for VS 2022
```

如果找不到这些入口，通常说明 C++ 工作负载没有安装完整。

## 安装 CMake

安装 Windows x64 版本 CMake。

安装时建议勾选：

```text
Add CMake to the system PATH for all users
```

安装完成后打开 PowerShell：

```powershell
cmake --version
```

能看到版本号即可。

## 安装 MySQL Server 5.7

MySQL Server 用于运行服务端数据库。编译阶段使用的是 MySQL 客户端开发库，运行阶段需要 MySQL 服务。

推荐安装：

```text
MySQL Server 5.7 x64
```

安装时请记录 root 密码，后续导入数据库和修改配置文件需要使用。

建议同时安装数据库管理工具，例如 HeidiSQL。连接本机 MySQL 时通常使用：

```text
主机：127.0.0.1
端口：3306
用户：root
密码：安装 MySQL 时设置的密码
```

## 获取源码

创建项目父目录：

```powershell
New-Item -ItemType Directory -Force -Path D:\WYTurtle
```

克隆仓库：

```powershell
git clone <仓库地址> D:\WYTurtle\source
```

示例：

```powershell
git clone https://github.com/your-name/WYTurtle.git D:\WYTurtle\source
```

克隆完成后确认文件存在：

```text
D:\WYTurtle\source\CMakeLists.txt
```

也可以使用 GitHub Desktop 克隆，目标目录同样选择：

```text
D:\WYTurtle\source
```

## 准备 ACE

本项目需要 ACE。推荐目录：

```text
D:\WYTurtle\deps\ACE_wrappers
```

准备步骤：

1. 下载 ACE 源码包。
2. 解压到 `D:\WYTurtle\deps\ACE_wrappers`。
3. 确认存在：

```text
D:\WYTurtle\deps\ACE_wrappers\ace
D:\WYTurtle\deps\ACE_wrappers\ACE_vs2019.sln
```

4. 使用 Visual Studio 2022 打开：

```text
D:\WYTurtle\deps\ACE_wrappers\ACE_vs2019.sln
```

5. 如果 Visual Studio 提示升级项目，选择允许。
6. 顶部配置选择：

```text
配置：Release
平台：x64
```

7. 执行 `生成` -> `生成解决方案`。
8. 编译完成后确认存在：

```text
D:\WYTurtle\deps\ACE_wrappers\lib\ACE.lib
D:\WYTurtle\deps\ACE_wrappers\lib\ACE.dll
```

如果没有 `ACE.lib` 或 `ACE.dll`，说明 ACE 没有编译成功，需要先解决 ACE 编译问题。

## 准备 Windows 第三方依赖库

Windows 编译需要 MySQL、OpenSSL、Zlib 等依赖库。头文件可以放在源码仓库中，但 `.lib` 和 `.dll` 通常不提交到 Git。

推荐依赖库目录：

```text
D:\WYTurtle\deps\dep-windows-lib
```

目录结构建议如下：

```text
D:\WYTurtle\deps\dep-windows-lib
├─ x64_release
│  ├─ libmysql.lib
│  ├─ libmySQL.dll
│  ├─ libssl.lib
│  ├─ libssl-1_1-x64.dll
│  ├─ libcrypto.lib
│  ├─ libcrypto-1_1-x64.dll
│  ├─ libeay32.dll
│  ├─ OptickCore.lib
│  └─ OptickCore.dll
└─ x64_Debug
   ├─ libmysql.lib
   ├─ libmySQL.dll
   ├─ libssl.lib
   ├─ libssl-1_1-x64.dll
   ├─ libcrypto.lib
   └─ libcrypto-1_1-x64.dll
```

说明：

```text
RelWithDebInfo / Release / MinSizeRel 使用 x64_release
Debug 使用 x64_Debug
```

初次编译建议使用 `RelWithDebInfo`，这样只要 `x64_release` 完整即可。

头文件默认位置：

```text
D:\WYTurtle\source\dep\windows\include\mysql
D:\WYTurtle\source\dep\windows\include\openssl
D:\WYTurtle\source\dep\windows\include\zlib
```

如果你的仓库版本没有这些头文件，需要从依赖包中补齐。

## 命令行编译

### 1. 打开 VS 开发命令行

从开始菜单打开：

```text
x64 Native Tools Command Prompt for VS 2022
```

也可以打开普通命令行后执行：

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
```

如果安装的是 Professional、Enterprise 或 BuildTools，请把路径中的 `Community` 替换成对应版本。

### 2. 创建构建目录

```bat
mkdir D:\WYTurtle\server
mkdir D:\WYTurtle\build\relwithdebinfo
```

### 3. 配置 CMake

```bat
cmake -S "D:\WYTurtle\source" -B "D:\WYTurtle\build\relwithdebinfo" -G "NMake Makefiles" ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_INSTALL_PREFIX="D:/WYTurtle/server" ^
  -DACE_ROOT="D:/WYTurtle/deps/ACE_wrappers" ^
  -DWINDOWS_DEP_LIB_DIR="D:/WYTurtle/deps/dep-windows-lib" ^
  -DUSE_LUA=ON ^
  -DUSE_SCRIPTS=ON
```

配置成功时会看到：

```text
Configuring done
Generating done
Build files have been written to: D:/WYTurtle/build/relwithdebinfo
```

不要在源码目录中生成构建文件。不要执行 in-source build。

推荐使用：

```text
-DCMAKE_INSTALL_PREFIX=...
```

不要使用旧式 `-DPREFIX=...`。

### 4. 编译并安装

```bat
cmake --build "D:\WYTurtle\build\relwithdebinfo" --target INSTALL
```

第一次完整编译可能需要 3 到 10 分钟。编译完成后，输出会安装到：

```text
D:\WYTurtle\server
```

成功后应包含：

```text
D:\WYTurtle\server\realmd.exe
D:\WYTurtle\server\mangosd.exe
D:\WYTurtle\server\lua52_compiler.exe
D:\WYTurtle\server\lua52_interpreter.exe
D:\WYTurtle\server\realmd.conf.dist
D:\WYTurtle\server\mangosd.conf.dist
```

## 复制运行文件

编译安装后，需要把运行 DLL 复制到 `D:\WYTurtle\server`。

```bat
copy /y "D:\WYTurtle\deps\ACE_wrappers\lib\ACE.dll" "D:\WYTurtle\server\"
copy /y "D:\WYTurtle\deps\dep-windows-lib\x64_release\libmySQL.dll" "D:\WYTurtle\server\"
copy /y "D:\WYTurtle\deps\dep-windows-lib\x64_release\libssl-1_1-x64.dll" "D:\WYTurtle\server\"
copy /y "D:\WYTurtle\deps\dep-windows-lib\x64_release\libcrypto-1_1-x64.dll" "D:\WYTurtle\server\"
copy /y "D:\WYTurtle\deps\dep-windows-lib\x64_release\libeay32.dll" "D:\WYTurtle\server\"
copy /y "D:\WYTurtle\deps\dep-windows-lib\x64_release\OptickCore.dll" "D:\WYTurtle\server\"
```

RelWithDebInfo 模式建议同时复制 PDB：

```bat
copy /y "D:\WYTurtle\build\relwithdebinfo\bin\mangosd.pdb" "D:\WYTurtle\server\"
copy /y "D:\WYTurtle\build\relwithdebinfo\bin\realmd.pdb" "D:\WYTurtle\server\"
```

## 使用 CMake GUI 编译

也可以使用 CMake GUI。

### 1. 填写源码和构建目录

`Where is the source code`：

```text
D:\WYTurtle\source
```

`Where to build the binaries`：

```text
D:\WYTurtle\build\relwithdebinfo
```

### 2. Configure

点击 `Configure`。

如果弹出生成器选择：

```text
Generator: NMake Makefiles
Use default native compilers
```

也可以选择：

```text
Visual Studio 17 2022
Platform: x64
```

如果使用 Visual Studio 生成器，后续编译需要在 Visual Studio 中生成 `INSTALL` 项目。

### 3. 设置变量

确认或新增以下变量：

```text
CMAKE_BUILD_TYPE        RelWithDebInfo
CMAKE_INSTALL_PREFIX    D:/WYTurtle/server
ACE_ROOT                D:/WYTurtle/deps/ACE_wrappers
WINDOWS_DEP_LIB_DIR     D:/WYTurtle/deps/dep-windows-lib
USE_LUA                 ON
USE_SCRIPTS             ON
```

再次点击 `Configure`，直到没有红色错误。

### 4. Generate

点击 `Generate`。

### 5. 编译

如果使用 `NMake Makefiles`：

```bat
cmake --build "D:\WYTurtle\build\relwithdebinfo" --target INSTALL
```

如果使用 `Visual Studio 17 2022`：

1. 点击 CMake GUI 的 `Open Project`。
2. Visual Studio 顶部选择 `RelWithDebInfo` 和 `x64`。
3. 在解决方案资源管理器中找到 `INSTALL`。
4. 右键 `INSTALL`。
5. 点击 `生成`。

## 数据库准备

编译成功只代表服务端程序已经生成。真正启动服务端还需要数据库。

通常需要以下数据库：

```text
realmd 或 logon
characters
world
logs
```

常见 SQL 文件：

```text
sql\_structure_logon.sql
sql\_structure_characters.sql
sql\_structure_logs.sql
sql\database_updates\*.sql
```

world 基础数据库可能因为体积或授权原因不随源码仓库上传。需要准备与该核心匹配的 Turtle/MaNGOS 1.12 world 数据库。

基本流程：

1. 启动 MySQL Server 5.7。
2. 创建登录库、角色库、世界库和日志库。
3. 导入结构 SQL。
4. 导入 world 基础数据库。
5. 按顺序导入 `sql\database_updates` 中需要的更新 SQL。
6. 修改服务端配置文件中的数据库连接信息。

如果 SQL 导入时报错，不建议跳过。数据库不完整时，世界服很可能无法启动。

## 配置服务端

安装目录中会有：

```text
D:\WYTurtle\server\realmd.conf.dist
D:\WYTurtle\server\mangosd.conf.dist
```

复制成正式配置文件：

```bat
copy "D:\WYTurtle\server\realmd.conf.dist" "D:\WYTurtle\server\realmd.conf"
copy "D:\WYTurtle\server\mangosd.conf.dist" "D:\WYTurtle\server\mangosd.conf"
```

打开 `realmd.conf` 和 `mangosd.conf`，修改数据库连接。

连接字符串通常类似：

```text
127.0.0.1;3306;用户名;密码;数据库名
```

示例：

```text
127.0.0.1;3306;root;你的密码;realmd
127.0.0.1;3306;root;你的密码;characters
127.0.0.1;3306;root;你的密码;world
127.0.0.1;3306;root;你的密码;logs
```

具体配置项名称以配置文件内容为准。

## 启动服务端

建议使用命令行启动，不要直接双击 exe。命令行可以看到错误输出。

启动登录服：

```bat
cd /d D:\WYTurtle\server
realmd.exe
```

再开一个命令行窗口，启动世界服：

```bat
cd /d D:\WYTurtle\server
mangosd.exe
```

正常情况下，`realmd.exe` 和 `mangosd.exe` 应分别保持运行。

## Lua 脚本

Lua 脚本不随源码仓库提供。需要使用 Lua 时，在运行目录创建：

```text
D:\WYTurtle\server\lua_scripts
```

在 `mangosd.conf` 中确认：

```text
Eluna.Enabled = 1
Eluna.ScriptPath = "lua_scripts"
```

检查 Lua 脚本语法：

```bat
D:\WYTurtle\server\lua52_compiler.exe -p D:\WYTurtle\server\lua_scripts\你的脚本.lua
```

普通 Lua 脚本修改不需要重编译 C++。修改 `src\game\LuaEngine` 或其他 C++ 源码才需要重新编译。

## 发布给其他人的建议

为了让其他人顺利编译，建议发布时同时提供：

```text
WYTurtle 源码仓库
Windows 依赖库压缩包
ACE 编译说明
数据库说明
```

Windows 依赖库压缩包建议解压后得到：

```text
D:\WYTurtle\deps\dep-windows-lib
```

如果不想固定盘符，也可以让对方放到其他目录，但配置 CMake 时必须修改：

```text
-DWINDOWS_DEP_LIB_DIR=实际依赖库目录
```

## 常见问题

### CMake 提示找不到 ACE

确认存在：

```text
D:\WYTurtle\deps\ACE_wrappers\ace
D:\WYTurtle\deps\ACE_wrappers\lib\ACE.lib
D:\WYTurtle\deps\ACE_wrappers\lib\ACE.dll
```

如果不存在，先重新编译 ACE。

### 提示找不到 libmysql.lib

确认存在：

```text
D:\WYTurtle\deps\dep-windows-lib\x64_release\libmysql.lib
```

如果编译 Debug，还要确认：

```text
D:\WYTurtle\deps\dep-windows-lib\x64_Debug\libmysql.lib
```

如果依赖库放在其他位置，配置 CMake 时设置：

```text
-DWINDOWS_DEP_LIB_DIR=实际依赖库目录
```

### 提示找不到 dep\windows\lib

说明正在使用旧的 CMake 缓存或旧的构建目录。

删除构建目录：

```bat
rmdir /s /q D:\WYTurtle\build\relwithdebinfo
```

然后重新执行 CMake 配置命令，确保包含：

```text
-DWINDOWS_DEP_LIB_DIR=D:/WYTurtle/deps/dep-windows-lib
```

### CMakeCache 配置错了

删除构建目录后重新配置：

```bat
rmdir /s /q D:\WYTurtle\build\relwithdebinfo
```

不要删除源码目录。

### Debug 编译失败

Debug 需要 Debug 版本第三方库。初次编译建议使用：

```text
RelWithDebInfo
```

### exe 无法写入或链接失败

确认以下程序没有运行：

```text
realmd.exe
mangosd.exe
lua52_compiler.exe
lua52_interpreter.exe
```

关闭后重新编译。

### 服务端启动后立即退出

优先检查：

```text
realmd.conf / mangosd.conf 是否存在
数据库连接是否正确
运行目录是否包含 ACE.dll、libmySQL.dll、OpenSSL DLL
world / characters / realmd / logs 数据库是否完整
端口是否被占用
```

### 源码目录出现 build、bin、install 或日志

构建目录不应该放在源码目录中。建议统一使用：

```text
D:\WYTurtle\build\relwithdebinfo
```

如果误生成了构建产物，确认没有需要保留的文件后可以删除。
