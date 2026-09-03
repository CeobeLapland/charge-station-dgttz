/* always-apply true */

# AGENTS_ALWAYS

- 这是一个全栈项目，我们负责的是机器学习子系统，子目录在 `machine_learning/`
- 内容可以参考根目录下的 docs 文件夹，数据库表结构以根目录 `DATA_STRUCTURE.md` 和 `docs/content/spec‑数据库.md` 为准
- 通信协议以 `docs/content/spec‑协议.md` 为准
- 请把依赖、构建产物与模型文件放在 `machine_learning/` 子目录下，以免产生依赖冲突
- 训练数据优先从数据库读取，本地仿真数据集也放在 `machine_learning/` 下
- 一般情况下，不要去其他子文件夹读取操作文件，我们只负责 `machine_learning/`
