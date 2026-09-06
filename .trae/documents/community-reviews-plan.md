# 社区界面 + 评论功能增强 实施方案

## Context（背景与目标）

探索页的「社区」目前是占位 sheet（`ExplorePage.qml` 的 `placeholderSheet`）。本次要：

1. **先增强评论功能**：当前用户只能看评论（只读种子数据），不能发评论、不能点赞「有用」、不能回复。需补齐这三项交互。
2. **再实现「社区」子界面**：把散落在各电站下的评论聚合到一个界面，支持筛选/排序。

关键事实（已确认）：
- 「电站详情」在两处各有独立的 QML 实现，评论渲染代码各自复制一份：
  - 全页详情：`user/qml/pages/StationDetailPage.qml`
  - 探索地图内底部面板（sheet）：`user/qml/pages/ExplorePage.qml#L568` 的 `stationDetailSheet`
- 评论数据源：`ExploreData::reviewsForStation()`（C++ 单例，`user/src/ExploreData.cpp`），目前是只读种子数据，评价 map **没有 `id` 字段**、无可变状态。
- 用户已确认范围：
  - 交互 = 点赞（有用）+ 回复评论（嵌套子评论）
  - 发评论入口 = 仅两处电站详情页提供「写评价」
  - 社区筛选 = 最新(时间) / 最热(有用数) / 综合评分 +「只看我的」

## 方案

### 1. 数据层（C++，`ExploreData` 变可写）

在 `user/src/ExploreData.h` / `.cpp` 为「电站评价」引入运行时可变仓库，保持现有 C++ 单例 + `Q_INVOKABLE` + signal 刷新风格（对齐 `UserData::toggleFavorite`/`addVehicle`）。

**`ExploreData.h` 新增声明：**

```cpp
signals:
    void reviewsChanged();                     // 发评论/点赞/回复后通知 QML 刷新

// 某电站评价（合并种子 + 用户新发，每条含 id/station_name/isMine/likedByMe/replies）
QVariantList reviewsForStation(int stationId) const;
// 全部电站评价，供社区页聚合。（改动原只读方法）
// sortMode: 0=最新 1=最热(有用数) 2=综合评分；mineOnly=true 只含用户新发
QVariantList allReviews(int sortMode, bool mineOnly) const;
// 写评价（QML 组装好字段传入），返回成功
bool addReview(const QVariantMap& r);
// 点赞/取消「有用」，返回最新 useful_count
int toggleUseful(int reviewId);
// 追加回复，author 为当前用户昵称
bool addReply(int reviewId, const QString& author, const QString& content);
```

**`ExploreData.cpp` 实现要点：**

- 新增一个懒构建的一次性仓库，把种子电站（`buildSeeds()` 静态）里的每条评价展开为运行时记录，并分配稳定递增 id（种子评价原本无 id）。
  - 运行时记录：`{id, stationId, stationName, base(原字段), mine=false, likedByMe, usefulCount, replies[]}`。
  - `makeReviews()` 保持不变，但仓库记录里补上 `id / station_id / station_name`，以对齐 `review` 表（DATA_STRUCTURE.md 的 `review` 有 `id/user_id/station_id`）。
- 用户新发评论：`addReview` 从传入 map 读取 `station_id`、6 维评分 `overall/speed/device/parking/hygiene/service`、`tags`、`content`、`nickname`；`create_time` 取当前时间 `yyyy-MM-dd HH:mm:ss`；`id` 继续递增；`mine=true`；追加到仓库并 `emit reviewsChanged()`。`station_name` 由 `stationById()` 兜底填充。
- `toggleUseful(id)`：命中的记录翻转 `likedByMe`，`useful_count` ±1，返回新值，`emit reviewsChanged()`。
- `addReply(id, author, content)`：往该记录的 `replies[]` 追加 `{id, author, content, create_time}`，`emit reviewsChanged()`。
- `reviewsForStation()`：改为过滤产物（种子+用户新发），每条转化为带 `id/station_name/isMine/likedByMe/useful_count/replies` 的 map。
- `allReviews(sortMode, mineOnly)`：聚合全仓库，`mineOnly` 过滤，按时间串/`useful_count`/`overall_score` 排序后返回。时间排序用 `QDateTime::fromString(create_time,"yyyy-MM-dd HH:mm:ss")`（对秒缺省值按 0 处理，种子是否为纯展示排序影响不大，统一用字符串亦可）。
- 注册无需改动（`main.cpp` 已 `qmlRegisterSingletonInstance`，新增 signal 自动暴露）。

### 2. 电站详情页增强（两处同步）

给 `StationDetailPage.qml` 与 `ExplorePage.qml` 的 `stationDetailSheet` 各自加：

- **评价列表交互**：每条评论卡片加
  - 「有用 N」可点击按钮 → 调 `ExploreData.toggleUseful(id)`，本地即时变更显示（含已赞态）。
  - 「回复」入口 → 展开输入框，提交调 `ExploreData.addReply(id, profile.nickname, content)`；回复以缩进子评论形式展示在该条下方。
- **「写评价」入口**：详情页顶部评价区加一个「写评价」按钮，打开一个 Popup 表单：
  - 6 个维度评分（综合/速度/设备/停车/卫生/服务），用内联 `StarRating` 组件（5 星可点）。
  - 快捷标签 chips（可多选，预置「充电快/车位多/位置方便/免费停车/设备老旧/排队严重/适合快充/晚上人少」等枚举）。
  - `TextArea` 文字评价。
  - 提交 → `addReview({station_id, overall_score,...,tags,content,nickname})` → toast「评价已发布」。
- **刷新机制**：把 `readonly property var reviews` 改为 `property var reviews: []`，并加 `function reloadReviews(){ reviews = ExploreData.reviewsForStation(stationId) }`，用 `Connections { target: ExploreData; function onReviewsChanged(){ reloadReviews() } }` 触发。`Component.onCompleted` 调一次 `reloadReviews()`。
- 在两次详情的评价卡片中统一使用当前用户 `UserData.profile().nickname`。

> 说明：这个内联 `StarRating` 与「写评价」表单在两处详情各有一份（为保持与现有 `stationDetailSheet` 内联 component 的组织一致，不另建跨页公共组件；如后续重复再抽公共件）。社区页同样内联在 `ExplorePage` 内，**不新增 QML 文件，故无需改 CMakeLists**。

### 3. 社区页（ExplorePage 内新 `communitySheet`）

- 修改 `sheetLoader` 的 `sourceComponent` 分发：`activeSheet === "community"` → 返回新增的 `communitySheet`（替换原 `placeholderSheet`）。
- `communitySheet`（内联 `Component`）：
  - 顶部返回按钮（`closeSheet`）+ 标题「社区」。
  - 排序/筛选栏：三个排序 chip（最新/最热/评分）+ 「只看我的」勾选 + 评论总数。
  - `property int sortMode` / `property bool mineOnly` / `property var communityReviews`；当 `sortMode`/`mineOnly`/`ExploreData.reviewsChanged` 变化时 `communityReviews = ExploreData.allReviews(sortMode, mineOnly)`。
  - 可滚动 `ListView` 渲染评论卡片：带头像/昵称、所属电站名 chip、⭐综合分、文字、标签 chips、「有用 N」可点、时间、展开式回复列表 + 回复输入。复用与详情页一致的交互（点赞、回复）。
  - 点击电站名可在社区内预选该站（可选，次要字段：`station_name` 已含，可加"去电站详情"动作——先不做，避免扩大范围）。

### 涉及文件

- `user/src/ExploreData.h` —— 新增声明
- `user/src/ExploreData.cpp` —— 仓库 + 新方法实现
- `user/qml/pages/ExplorePage.qml` —— 社区 sheet + `stationDetailSheet` 评论交互/写评价
- `user/qml/pages/StationDetailPage.qml` —— 全页详情评论交互/写评价

不改：`UserData`（「只看我的」用 `ExploreData` 的 `isMine`，与 `UserData.myReviews()` 独立）、`CMakeLists.txt`、协议/数据库规格文档。

## 验证（端到端）

1. `cmake -S . -B build && cmake --build build -j`。
2. `./build/user_client` 手动验证：
   - 首页→电站列表→任意站详情；探索→地图→marker 详情：两处均可（a）点「有用」计数 +1 且再次点击可取消；（b）回复评论并即时显示；（c）点「写评价」提交后列表顶部出现新评论。
   - 探索→社区：聚合了所有电站评论与新建评论；切换「最新/最热/评分」排序正确；勾选「只看我的」只留当前用户新发评论；对社区内评论也可点赞/回复。
   - 发评论/点赞/回复后返回再进，状态保持一致（同一次运行内存态）。
3. 编译无新文件，`CMakeLists` 不需改动，确认无 CMake 报错。