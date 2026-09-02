#include <QCoreApplication>
#include <QTextStream>

#include "Database.h"
#include "StationDao.h"
#include "UserDao.h"

// ============================================================
// 第一阶段 main: DAO 层自测程序
// 用法: ./charge_server [数据库路径]   (默认 ../sql/charge.db, 即在 build/ 里运行)
// 后续阶段这里会换成 WebSocket 服务主循环, DAO 层原样保留。
// ============================================================
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QString dbPath = (argc > 1) ? QString::fromLocal8Bit(argv[1])
                                      : QStringLiteral("../sql/charge.db");
    if (!db::open(dbPath)) {
        out << "[失败] " << db::lastError() << "\n";
        return 1;
    }
    out << "[OK] 数据库已连接: " << dbPath << "\n\n";

    // ---- 测试1: 查询种子数据里的老用户 ----
    if (auto u = dao::findUserByPhone(QStringLiteral("13800000001"))) {
        out << "[OK] 老用户查询: " << u->nickname
            << "  余额=" << u->balance << "  积分=" << u->points << "\n";
    } else {
        out << "[失败] 找不到种子用户 13800000001\n";
    }

    // ---- 测试2: 新手机号自动注册(跑第二遍时应变成'直接登录') ----
    bool created = false;
    if (auto u = dao::loginOrRegister(QStringLiteral("13912345678"), &created)) {
        out << "[OK] 登录/注册: " << u->nickname
            << (created ? "  (本次新注册)" : "  (已存在, 直接登录)") << "\n";
    } else {
        out << "[失败] loginOrRegister 出错\n";
    }

    // ---- 测试3: 非法手机号应被拒绝 ----
    if (!dao::loginOrRegister(QStringLiteral("123"))) {
        out << "[OK] 非法手机号被正确拒绝\n";
    } else {
        out << "[失败] 非法手机号竟然登录成功\n";
    }

    // ---- 测试4: 电站列表 + 实时空闲数 ----
    out << "\n电站列表:\n";
    for (const auto &s : dao::listStations()) {
        out << QStringLiteral("  #%1 %2 [%3]  桩:%4台/空闲%5  服务费%6元/kWh\n")
                   .arg(s.id).arg(s.name, s.area)
                   .arg(s.totalChargers).arg(s.freeChargers).arg(s.serviceFee);
    }

    out << "\nDAO 自测完成。\n";
    return 0;
}
