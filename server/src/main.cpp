#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include "Database.h"
#include "StationDao.h"
#include "UserDao.h"
#include "WsServer.h"

// ============================================================
// 服务端入口
//   ./charge_server                      → 启动 WebSocket 服务(端口9000, 库 ../sql/charge.db)
//   ./charge_server --db 路径            → 指定数据库文件
//   ./charge_server --port 9000          → 指定端口
//   ./charge_server --selftest           → 只跑 DAO 自测, 不启服务
// ============================================================

static int runSelfTest()
{
    QTextStream out(stdout);

    if (auto u = dao::findUserByPhone(QStringLiteral("13800000001")))
        out << "[OK] 老用户查询: " << u->nickname << "  余额=" << u->balance << "\n";
    else
        out << "[失败] 找不到种子用户 13800000001\n";

    bool created = false;
    if (auto u = dao::loginOrRegister(QStringLiteral("13912345678"), &created))
        out << "[OK] 登录/注册: " << u->nickname
            << (created ? "  (本次新注册)" : "  (已存在, 直接登录)") << "\n";

    if (!dao::loginOrRegister(QStringLiteral("123")))
        out << "[OK] 非法手机号被正确拒绝\n";

    out << "\n电站列表:\n";
    for (const auto &s : dao::listStations())
        out << QStringLiteral("  #%1 %2 [%3]  桩:%4台/空闲%5  当前电价%6元/kWh\n")
                   .arg(s.id).arg(s.name, s.area).arg(s.totalChargers)
                   .arg(s.freeChargers).arg(dao::currentPrice(s.id));

    out << "\nDAO 自测完成。\n";
    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    // 简易命令行参数解析
    const QStringList args = app.arguments();
    QString dbPath = QStringLiteral("../sql/charge.db");
    quint16 port = 9000;   // spec-协议.md 待定项建议初值
    bool selftest = false;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QStringLiteral("--db") && i + 1 < args.size())
            dbPath = args[++i];
        else if (args[i] == QStringLiteral("--port") && i + 1 < args.size())
            port = args[++i].toUShort();
        else if (args[i] == QStringLiteral("--selftest"))
            selftest = true;
    }

    if (!db::open(dbPath)) {
        out << "[失败] " << db::lastError() << "\n";
        return 1;
    }
    qInfo().noquote() << QStringLiteral("[OK] 数据库已连接: %1").arg(dbPath);

    if (selftest)
        return runSelfTest();

    WsServer server(port);
    if (!server.isListening())
        return 2;

    qInfo().noquote() << QStringLiteral("服务端运行中, 等待各端连接... (Ctrl+C 退出)");
    return app.exec();   // 进入Qt事件循环: 从此程序"挂机"等消息, 收发全靠信号槽驱动
}
