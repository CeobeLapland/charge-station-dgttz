#include "Database.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace db {

static QString g_lastError;

bool open(const QString &dbPath)
{
    // 1. 数据库文件必须已存在(由 make_seed.py 生成)。
    //    QSQLITE 驱动对不存在的路径会"自动新建空库", 那会导致查什么都没有,
    //    很难排查, 所以这里先挡住。
    if (!QFile::exists(dbPath)) {
        g_lastError = QStringLiteral(
            "数据库文件不存在: %1\n请先在 server/sql/ 下运行: python3 make_seed.py").arg(dbPath);
        return false;
    }

    // 2. 建立默认连接。之后代码里 QSqlQuery q; 不带参数就用这条连接。
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(dbPath);
    if (!database.open()) {
        g_lastError = QStringLiteral("打开数据库失败: %1").arg(database.lastError().text());
        return false;
    }

    // 3. SQLite 的外键约束默认是关闭的, 每条连接都要显式打开 —— 否则
    //    schema.sql 里写的所有 REFERENCES 都形同虚设。
    QSqlQuery pragma;
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    // 4. 抽查一张核心表, 确认库是建好的(而不是一个意外产生的空库)。
    QSqlQuery check;
    if (!check.exec(QStringLiteral(
            "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='user'"))
        || !check.next() || check.value(0).toInt() != 1) {
        g_lastError = QStringLiteral("库里没有业务表, 请先运行 make_seed.py 重新生成 charge.db");
        return false;
    }
    return true;
}

QString lastError() { return g_lastError; }

}  // namespace db
