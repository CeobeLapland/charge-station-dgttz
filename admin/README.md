# 服务器管理端（admin/）

Linux + Qt Widgets 宽屏后台程序，面向运营管理员。功能定义见《02-服务器管理端》产品文档，
数据结构以根目录 `DATA_STRUCTURE.md` 为准，协议见 `docs/content/spec-协议.md`（暂存草稿：`src/network/Protocol.h`）。

## 依赖（在 admin 子目录内隔离）

| 模块 | 包名（Ubuntu 22.04） | 用途 |
| --- | --- | --- |
| Qt6 Widgets | qt6-base-dev | 界面 |
| Qt6 Network | qt6-base-dev | 网络 |
| Qt6 Charts | libqt6charts6-dev | QChart 营收折线图 |
| Qt6 WebSockets | libqt6websockets6-dev | 与服务端进程通信 |

```bash
sudo apt install libqt6websockets6-dev libqt6charts6-dev
```

## 构建

```bash
cd admin
cmake -S . -B build
cmake --build build -j
./build/admin_client
```

## 运行模式

- **Mock 模式（默认）**：服务端进程未就绪时，用 `MockDataProvider` 假数据独立跑通界面。
  登录默认 `admin / 123456`。
- **真实模式**：服务端就绪后，把 `src/AppConfig.h` 的 `kServerUrl` 指向真实地址，
  `ApiClient` 自动切换为 WebSocket 数据源，不需要改页面代码。

## 目录结构

```
admin/
├── CMakeLists.txt            # 独立 CMake 工程
├── resources/                # 深色 QSS 主题（resources.qrc）
└── src/
    ├── main.cpp
    ├── AppConfig.h           # 服务端地址、心跳等配置
    ├── network/              # Protocol（协议草稿）、ServerConnection（WebSocket）、MockDataProvider
    ├── services/             # ApiClient：业务请求统一入口
    └── ui/                   # LoginDialog、MainWindow（左侧导航）、pages/（各功能页）
```

> 协议常量当前暂存于 admin，待 `common/` 定稿后迁移到 common/ 统一引用（禁止双份维护）。


## Windows 开发环境（Qt 6.2.4 · D:\QT）

已用 aqtinstall 安装到 D:\QT（与队友 Ubuntu 上的 Qt 6.2.4 同版本）：

| 组件 | 路径 |
| --- | --- |
| Qt 6.2.4（MinGW 64 位，含 Charts/WebSockets） | `D:\QT\6.2.4\mingw_64` |
| MinGW 13.1 工具链 | `D:\QT\Tools\mingw1310_64` |
| Qt Creator | `D:\QT\Tools\QtCreator\bin\qtcreator.exe` |
| CMake | `D:\QT\Tools\CMake_64\bin\cmake.exe` |

**Qt Creator 打开**：启动 `D:\QT\Tools\QtCreator\bin\qtcreator.exe` → File → Open File or Project → 选本目录 `CMakeLists.txt` → 选 Kit **Desktop Qt 6.2.4 MinGW 64-bit**（未自动识别就手动建：编译器 `D:\QT\Tools\mingw1310_64\bin\g++.exe`、Qt 版本 `D:\QT\6.2.4\mingw_64\bin\qmake.exe`、CMake `D:\QT\Tools\CMake_64\bin\cmake.exe`）→ 构建 Ctrl+B → 运行 Ctrl+R，登录 `admin / 123456`（Mock 模式，无需服务端）。

**命令行构建**：

```bat
set PATH=D:\QT\Tools\mingw1310_64\bin;%PATH%
D:\QT\Tools\CMake_64\bin\cmake.exe -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=D:\QT\6.2.4\mingw_64
D:\QT\Tools\CMake_64\bin\cmake.exe --build build -j8
:: 产物：build\admin_client.exe
```

> 注意：Qt 6 的 Charts 类在全局命名空间（`QChartView` / `QLineSeries`，没有 `QtCharts::` 前缀，Qt5 才有）。Linux 正式构建仍以 Ubuntu 22.04 + Qt 6.2 + CMake + C++17 为准。