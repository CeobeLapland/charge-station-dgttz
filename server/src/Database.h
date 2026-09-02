#pragma once
#include <QString>

// ============================================================
// Database — 数据库连接管理
// 职责: 打开 SQLite 数据库文件、启用外键约束、校验建库是否完成。
// 整个服务端进程只打开一次(默认连接), 之后所有 QSqlQuery 都自动用它。
// ============================================================
namespace db {

// 打开数据库。成功返回 true; 失败时用 lastError() 取原因。
// dbPath: charge.db 的路径 (由 server/sql/make_seed.py 生成)
bool open(const QString &dbPath);

// 最近一次 open 失败的原因说明(给日志/排错用)
QString lastError();

}  // namespace db
