# 用户端（user/）架构说明

> 本文简要说明充电用户端子项目的结构与设计思路。完整业务见根目录 `docs/spec-用户端.md`，协议见 `docs/spec-协议.md`，数据结构见 `DATA_STRUCTURE.md`。

## 一、总体架构：MVVM

QML 天然契合 MVVM，与 Android 的 MVVM 思路一致：

```
┌──────────── View（界面）─────────────┐
│  qml/pages/*.qml  qml/components/*   │
│  只负责展示 + 接收用户操作             │
└───────────────┬──────────────────────┘
                │ import / 属性绑定
┌──────────── ViewModel（逻辑）─────────┐
│  C++ 暴露给 QML 的类（如 UserClient） │
│  持有页面状态，调用后端               │
└───────────────┬──────────────────────┘
                │ QWebSocket + JSON(big信封type/seq/payload)
┌──────────── Model（数据）─────────────┐
│  服务端进程（非本子项目）             │
└──────────────────────────────────────┘
```

- **View**：纯 QML，不写业务逻辑。
- **ViewModel**：C++ 类经 `setContextProperty` 暴露（如 `backend` = `UserClient`），负责收发消息、维护页面状态，QML 通过信号/属性订阅。
- **Model**：统一走 `ws://127.0.0.1:9000` 的 [UserClient.cpp](file:///home/bit/projects/charge-station-dgttz/user/src/UserClient.cpp)（服务端默认端口，spec 待定项建议初值）。用户端不直连数据库。

## 二、目录结构

```
user/
├── CMakeLists.txt          # CMake 构建，Qt6 模块依赖，构建产物在 user/build（已 gitignore）
├── src/
│   ├── main.cpp            # 入口：启动引擎、挂 backend 到 QML、设置主题
│   ├── UserClient.h/.cpp   # 通信层：protocol 信封封装、连接到服务端、消息收发
│   └── （后续：各页面的 ViewModel / 领域模型）
└── qml/
    ├── Main.qml                    # 竖屏窗口 + StackView + 全局配色例子
    ├── qtquickcontrols2.conf       # Material 风格 + 主色
    ├── components/                 # 可复用控件（BottomNav 底部导航等）
    └── pages/                      # 页面
        └── （各个页面）
```

## 三、依赖与构建

- 依赖**隔离在 user/ 内**：`user/build` 存放构建产物；仅 include 仓库根 `common/` 的路径（当前为空，公共代码就绪后直接可用，不复制）。
- 构建命令（在 user/ 下执行）：

```bash
cmake -S . -B build
cmake --build build -j
./build/charge_user_client
```

- 可视化调试：用 Qt Creator 打开 `user/CMakeLists.txt`，运行 + QML 实时预览。

## 四、开发约定（对齐 AGENTS.md）

- 消息类型/字段/枚举：一律取自 `docs/spec-协议.md`、`spec-数据库.md`，不自造。
- 新增消息：先改协议文档再实现。
- C++ 命名：类/函数大驼峰、变量小驼峰、常量全大写蛇形；JSON 键小写蛇形。
- 待定项：按 spec 末尾建议初值实现，并注明（如端口 9000）。
- 真数据一律走服务端；前期可用「服务端种子数据」，避免把假数据写死进业务代码。

## 五、下一步

1. 抽取全局 `Theme.qml` 单例统一色调/字体/圆角。
2. 启动流程改为先进登录页 → 登录成功跳首页。
3. 按页面逐个填充功能（对接对应消息）。