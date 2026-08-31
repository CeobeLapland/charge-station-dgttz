# DEPENDENCE.md — 依赖清单

> 新增任何依赖前先读 `docs/AGENTS.md`（`common/`、`spec-*` 属于串行冲突点）。
> 状态核查时间：2026-08-31 · 环境：Ubuntu 22.04（VMware 虚拟机）。
> 原则：**已安装**的写版本；**待安装**的只列命令，本次不自动下载（虚拟机较卡，由组员按需手动执行）。

---

## 一、系统与构建工具

| 依赖 | 版本 | 用途 | 状态 |
| --- | --- | --- | --- |
| git | 2.34.1 | 版本协作 | 已安装 |
| cmake | 3.22.1 | 构建（AGENTS 要求用 CMake） | 已安装 |
| g++（GCC） | 11.4.0 | C++17 编译 | 已安装 |
| make | （随 build-essential） | 构建驱动 | 已安装 |
| Qt Creator / qmake6 | Qt 6.2.4 | 开发 IDE / 检查 Qt 版本 | 已安装（qmake 仅作探测，构建走 CMake） |

## 二、Qt 6 模块（已安装）

基础安装方式：

```bash
sudo apt install qt6-base-dev qt6-declarative-dev qt6-tools-dev \
  libqt6sql6-sqlite libqt6serialport6-dev
```

已装模块清单（对应 CMake `find_package` 名称）：

| 模块 | CMake 名 | 用途 |
| --- | --- | --- |
| Qt Core / Gui / Widgets | Qt6Core / Qt6Gui / Qt6Widgets | 管理端 Widgets 界面 |
| Qt QML / Quick / QuickControls2 | Qt6Qml / Qt6Quick / Qt6QuickControls2 | 用户端 QML 界面 |
| Qt Network | Qt6Network | 基础网络（TCP/HTTP） |
| Qt SQL（含 SQLite 驱动） | Qt6Sql | QSQLite 数据存储 |
| Qt Concurrent / SerialPort | Qt6Concurrent / Qt6SerialPort | 并发 / 可选串口 |

## 三、前端 / 大屏（screen/）

| 依赖 | 用途 | 状态 |
| --- | --- | --- |
| ECharts | 大屏图表渲染 | 运行时引入：下载 `echarts.min.js` 到 `screen/vendor/`（已加入 .gitignore）或用 CDN |
| 腾讯地图 Web API（JS） | 地理编码 / 导航 | 运行时外部服务，需申请 Key，代码中从 `spec-*` 取 |

## 四、机器学习（可选）

| 依赖 | 用途 | 状态 |
| --- | --- | --- |
| C++ 规则 + 仿真 | 主方案，预测/调度/故障 | 无额外依赖，已满足 |
| Python FastAPI（加分端点） | 可选的 `/predict` JSON 接口 | **未安装**（见待安装清单） |

---

## 五、待安装依赖（本次未下载）

| # | 依赖 | 包名（Ubuntu 22.04） | 用途 | 必要性 | 安装命令 |
| --- | --- | --- | --- | --- | --- |
| 1 | **Qt6 WebSockets** | `libqt6websockets6-dev` | 全端 WebSocket 通信核心（服务端/用户端/管理端） | ⭐ 必装 | `sudo apt install libqt6websockets6-dev` |
| 2 | **Qt6 WebEngine** | `qt6-webengine-dev` | 用户端 QWebEngineView 加载腾讯地图导航 | ⭐ 必装（体积大、较慢） | `sudo apt install qt6-webengine-dev` |
| 3 | nodejs / npm | `nodejs npm` | 大屏本地化 ECharts 可选工具链 | 可选（用 CDN 则不需要） | `sudo apt install nodejs npm` |
| 4 | sqlite3 CLI | `sqlite3` | 调试查看 .db 文件 | 可选（Qt SQL 驱动已可用） | `sudo apt install sqlite3` |
| 5 | Python ML 依赖 | `python3-numpy` `python3-scikit-learn` + pip 的 `fastapi uvicorn` | 机器学习加分端点（B 方案） | 可选 | `pip3 install fastapi uvicorn numpy scikit-learn` |

> 注意：仓库当前**未包含** WebSockets/WebEngine，若直接 `cmake` 会因缺少 `Qt6WebSockets`、`Qt6WebEngine` 报错。组内某台机器装好后，把 `spec-*` 之外的真实安装差异记到这里。
