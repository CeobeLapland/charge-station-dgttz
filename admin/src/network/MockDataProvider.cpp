#include "network/MockDataProvider.h"

#include <QDateTime>
#include <QJsonValue>
#include <QPair>
#include <QStringList>
#include <QVector>

#include <algorithm>

#include "network/Protocol.h"

namespace {

QString now() {
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString dateOffset(int daysAgo) {
    return QDateTime::currentDateTime().addDays(-daysAgo).toString(QStringLiteral("yyyy-MM-dd"));
}

QJsonArray& stationCache() {
    static QJsonArray cache = [] {
        QJsonArray arr;
        const QVector<QPair<QString, QString>> seeds = {
            {QStringLiteral("科技园站"), QStringLiteral("高新区科技路 88 号")},
            {QStringLiteral("市中心站"), QStringLiteral("中山路 128 号")},
            {QStringLiteral("东软园区站"), QStringLiteral("软件园东门 6 号")},
            {QStringLiteral("机场高速站"), QStringLiteral("机场高速入口服务区")},
            {QStringLiteral("商业区站"), QStringLiteral("万象城地下停车场 B2")},
        };
        for (int i = 0; i < seeds.size(); ++i) {
            QJsonObject s;
            s.insert(QStringLiteral("id"), i + 1);
            s.insert(QStringLiteral("name"), seeds[i].first);
            s.insert(QStringLiteral("address"), seeds[i].second);
            s.insert(QStringLiteral("longitude"), 116.30 + i * 0.03);
            s.insert(QStringLiteral("latitude"), 39.90 + i * 0.02);
            s.insert(QStringLiteral("total_chargers"), 8 + (i % 3) * 2);
            s.insert(QStringLiteral("online_rate"), 82.0 + i * 3.5);
            // ---- 以下字段对齐 server/sql/schema.sql 的 station 表 ----
            s.insert(QStringLiteral("area"),
                     QStringList{QStringLiteral("高新区"), QStringLiteral("市中心"),
                                 QStringLiteral("软件园"), QStringLiteral("机场"),
                                 QStringLiteral("商业区")}.at(i % 5));
            s.insert(QStringLiteral("service_fee"), 0.4 + (i % 3) * 0.1);
            s.insert(QStringLiteral("parking_fee"), (i % 4 == 0) ? 0.0 : 2.0);
            s.insert(QStringLiteral("business_hours"), QStringLiteral("00:00-24:00"));
            QStringList facilities{QStringLiteral("washroom")};
            if (i % 2 == 0) facilities << QStringLiteral("wifi");
            if (i != 1) facilities << QStringLiteral("convenience_store");
            if (i % 3 == 0) facilities << QStringLiteral("rain_shelter");
            s.insert(QStringLiteral("facilities"), QJsonArray::fromStringList(facilities));
            s.insert(QStringLiteral("owner_type"),
                     QStringList{QStringLiteral("self_run"), QStringLiteral("franchise"),
                                 QStringLiteral("partner"), QStringLiteral("third_party")}.at(i % 4));
            s.insert(QStringLiteral("has_swap"), (i == 3) ? 1 : 0);
            arr.append(s);
        }
        return arr;
    }();
    return cache;
}

QJsonArray& chargerCache() {
    static QJsonArray cache = [] {
        QJsonArray arr;
        int id = 1;
        const QJsonArray stations = stationCache();
        for (const QJsonValue& sv : stations) {
            const QJsonObject s = sv.toObject();
            const int stationId = s.value(QStringLiteral("id")).toInt();
            const int total = s.value(QStringLiteral("total_chargers")).toInt();
            for (int j = 0; j < total; ++j) {
                QJsonObject c;
                const bool fast = (j % 3) != 0;
                const QChar codeLetter = QChar(static_cast<ushort>('A' + (j / 10) % 26));
                c.insert(QStringLiteral("id"), id);
                c.insert(QStringLiteral("code"),
                         QString(QStringLiteral("%1-%2")).arg(codeLetter).arg(j % 10 + 1, 2, 10, QChar('0')));
                c.insert(QStringLiteral("station_id"), stationId);
                c.insert(QStringLiteral("station_name"), s.value(QStringLiteral("name")).toString());
                c.insert(QStringLiteral("type"), fast ? QStringLiteral("fast") : QStringLiteral("slow"));
                c.insert(QStringLiteral("power"), fast ? (j % 2 ? 120.0 : 60.0) : (j % 2 ? 11.0 : 7.0));
                QString status;
                switch (id % 7) {
                    case 0: status = QStringLiteral("fault"); break;
                    case 1: status = QStringLiteral("offline"); break;
                    case 2: status = QStringLiteral("rebooting"); break;
                    case 3: status = QStringLiteral("reserved"); break;
                    case 4: status = QStringLiteral("charging"); break;
                    default: status = QStringLiteral("idle");
                }
                c.insert(QStringLiteral("status"), status);
                c.insert(QStringLiteral("temperature"), 28.0 + (id % 25));
                c.insert(QStringLiteral("comm_status"),
                         (id % 9 == 0) ? QStringLiteral("abnormal") : QStringLiteral("normal"));
                c.insert(QStringLiteral("health_score"), qMax(45, 100 - (id % 40)));
                c.insert(QStringLiteral("total_charge_count"), (id * 37) % 500);
                c.insert(QStringLiteral("total_charge_duration"), (id * 53) % 8000);
                // ---- 实时电气参数/故障码，对齐 server/sql/schema.sql 的 charger 表 ----
                double voltage = 0.0, current = 0.0;
                if (status == QStringLiteral("charging")) {
                    voltage = fast ? 500.0 + (id % 10) * 10.0 : 220.0;
                    current = fast ? 60.0 + (id % 30) : 16.0 + (id % 16);
                }
                c.insert(QStringLiteral("voltage"), voltage);
                c.insert(QStringLiteral("current"), current);
                c.insert(QStringLiteral("fault_code"),
                         status == QStringLiteral("fault")
                             ? QStringLiteral("E-%1").arg(300 + (id % 90), 4, 10, QChar('0'))
                             : QString());
                c.insert(QStringLiteral("created_time"), dateOffset(30 + (id % 400)));
                arr.append(c);
                ++id;
            }
        }
        return arr;
    }();
    return cache;
}

QJsonArray& userCache() {
    static QJsonArray cache = [] {
        QJsonArray arr;
        const QVector<QPair<QString, QString>> seeds = {
            {QStringLiteral("13800000001"), QStringLiteral("用户0001")},
            {QStringLiteral("13800000002"), QStringLiteral("用户0002")},
            {QStringLiteral("13800000003"), QStringLiteral("用户0003")},
            {QStringLiteral("13911112222"), QStringLiteral("老张")},
            {QStringLiteral("13733334444"), QStringLiteral("阿伟")},
            {QStringLiteral("13655556666"), QStringLiteral("小美")},
        };
        for (int i = 0; i < seeds.size(); ++i) {
            QJsonObject u;
            u.insert(QStringLiteral("id"), i + 1);
            u.insert(QStringLiteral("phone"), seeds[i].first);
            u.insert(QStringLiteral("nickname"), seeds[i].second);
            u.insert(QStringLiteral("balance"), 20.0 + i * 33.5);
            u.insert(QStringLiteral("points"), i * 120);
            u.insert(QStringLiteral("level"),
                     (i % 3 == 0) ? QStringLiteral("vip") : QStringLiteral("normal"));
            u.insert(QStringLiteral("status"),
                     (i == 5) ? QStringLiteral("frozen") : QStringLiteral("normal"));
            u.insert(QStringLiteral("register_time"), dateOffset(30 - i * 4));
            u.insert(QStringLiteral("avatar_path"), QString());
            u.insert(QStringLiteral("last_login_time"),
                     dateOffset(i) + QStringLiteral(" 21:%1:00").arg(10 + i, 2, 10, QChar('0')));
            arr.append(u);
        }
        return arr;
    }();
    return cache;
}

}  // namespace

QJsonObject MockDataProvider::okPayload(const QJsonObject& payload) {
    QJsonObject resp;
    resp.insert(proto::field::kCode, proto::code::Ok);
    resp.insert(proto::field::kMessage, QStringLiteral("ok"));
    resp.insert(proto::field::kPayload, payload);
    return resp;
}

QJsonObject MockDataProvider::errPayload(int code, const QString& message) {
    QJsonObject resp;
    resp.insert(proto::field::kCode, code);
    resp.insert(proto::field::kMessage, message);
    resp.insert(proto::field::kPayload, QJsonObject());
    return resp;
}

QJsonObject MockDataProvider::adminLogin(const QString& account, const QString& password) {
    if (account == QStringLiteral("admin") && password == QStringLiteral("123456")) {
        QJsonObject admin;
        admin.insert(QStringLiteral("id"), 1);
        admin.insert(QStringLiteral("account"), account);
        QJsonObject payload;
        payload.insert(QStringLiteral("admin"), admin);
        return okPayload(payload);
    }
    return errPayload(proto::code::AccountOrPasswordError, QStringLiteral("账号或密码错误"));
}

QJsonObject MockDataProvider::revenue(int days) {
    QJsonArray trend;
    double total = 0.0;
    for (int i = days - 1; i >= 0; --i) {
        const double amount = 1200.0 + ((i * 7919) % 2400) + (i == 0 ? 186.5 : 0);
        QJsonObject item;
        item.insert(QStringLiteral("date"), dateOffset(i));
        item.insert(QStringLiteral("amount"), amount);
        trend.append(item);
        total += amount;
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("trend"), trend);
    payload.insert(QStringLiteral("today"), trend.at(0).toObject().value(QStringLiteral("amount")).toDouble());
    payload.insert(QStringLiteral("month"), total);
    payload.insert(QStringLiteral("total"), total * 3.2);
    return okPayload(payload);
}

QJsonObject MockDataProvider::stationStatus() {
    int charging = 0, idle = 0, fault = 0, offline = 0, reserved = 0, rebooting = 0;
    const QJsonArray chargers = chargerCache();
    for (const QJsonValue& cv : chargers) {
        const QString status = cv.toObject().value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("charging")) ++charging;
        else if (status == QStringLiteral("idle")) ++idle;
        else if (status == QStringLiteral("fault")) ++fault;
        else if (status == QStringLiteral("offline")) ++offline;
        else if (status == QStringLiteral("reserved")) ++reserved;
        else if (status == QStringLiteral("rebooting")) ++rebooting;
    }
    QJsonObject dist;
    dist.insert(QStringLiteral("charging"), charging);
    dist.insert(QStringLiteral("idle"), idle);
    dist.insert(QStringLiteral("fault"), fault);
    dist.insert(QStringLiteral("offline"), offline);
    dist.insert(QStringLiteral("reserved"), reserved);
    dist.insert(QStringLiteral("rebooting"), rebooting);
    QJsonObject payload;
    payload.insert(QStringLiteral("distribution"), dist);
    payload.insert(QStringLiteral("total"), chargers.size());
    return okPayload(payload);
}

QJsonObject MockDataProvider::stations() {
    QJsonObject payload;
    payload.insert(QStringLiteral("stations"), stationCache());
    return okPayload(payload);
}

QJsonObject MockDataProvider::chargers(int stationId) {
    QJsonArray result;
    const QJsonArray chargers = chargerCache();
    for (const QJsonValue& cv : chargers) {
        const QJsonObject c = cv.toObject();
        if (stationId <= 0 || c.value(QStringLiteral("station_id")).toInt() == stationId) {
            result.append(c);
        }
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("chargers"), result);
    return okPayload(payload);
}

QJsonObject MockDataProvider::users(const QString& keyword) {
    QJsonArray result;
    const QJsonArray users = userCache();
    for (const QJsonValue& uv : users) {
        const QJsonObject u = uv.toObject();
        if (keyword.isEmpty()
            || u.value(QStringLiteral("phone")).toString().contains(keyword)
            || u.value(QStringLiteral("nickname")).toString().contains(keyword)) {
            result.append(u);
        }
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("users"), result);
    return okPayload(payload);
}

QJsonObject MockDataProvider::addStation(const QJsonObject& station) {
    QJsonArray& stations = stationCache();
    int nextId = 1;
    for (const QJsonValue& sv : stations) {
        nextId = qMax(nextId, sv.toObject().value(QStringLiteral("id")).toInt() + 1);
    }
    QJsonObject newStation = station;
    newStation.insert(QStringLiteral("id"), nextId);
    newStation.insert(QStringLiteral("online_rate"), 100.0);
    stations.append(newStation);
    QJsonObject payload;
    payload.insert(QStringLiteral("station"), newStation);
    return okPayload(payload);
}

QJsonObject MockDataProvider::chargerRestart(int chargerId) {
    QJsonArray& chargers = chargerCache();
    bool found = false;
    for (QJsonValueRef cv : chargers) {
        QJsonObject obj = cv.toObject();
        if (obj.value(QStringLiteral("id")).toInt() == chargerId) {
            obj.insert(QStringLiteral("status"), QStringLiteral("rebooting"));
            cv = obj;
            found = true;
            break;
        }
    }
    if (!found) {
        return errPayload(proto::code::DataNotFound, QStringLiteral("电桩不存在"));
    }
    QJsonObject deviceLog;
    deviceLog.insert(QStringLiteral("id"), chargerId * 100 + 1);
    deviceLog.insert(QStringLiteral("charger_id"), chargerId);
    deviceLog.insert(QStringLiteral("action"), QStringLiteral("restart"));
    deviceLog.insert(QStringLiteral("operator"), QStringLiteral("admin"));
    deviceLog.insert(QStringLiteral("op_time"), now());
    deviceLog.insert(QStringLiteral("result"), QStringLiteral("success"));
    QJsonObject payload;
    payload.insert(QStringLiteral("device_log"), deviceLog);
    return okPayload(payload);
}

QJsonObject MockDataProvider::toggleUserStatus(int userId, const QString& status) {
    QJsonArray& users = userCache();
    for (QJsonValueRef uv : users) {
        QJsonObject obj = uv.toObject();
        if (obj.value(QStringLiteral("id")).toInt() == userId) {
            obj.insert(QStringLiteral("status"), status);
            uv = obj;
            QJsonObject payload;
            payload.insert(QStringLiteral("user"), obj);
            return okPayload(payload);
        }
    }
    return errPayload(proto::code::DataNotFound, QStringLiteral("用户不存在"));
}

QJsonObject MockDataProvider::deviceLogs(int chargerId) {
    QJsonArray logs;
    const QStringList actions = {QStringLiteral("restart"), QStringLiteral("pause"), QStringLiteral("repair")};
    for (int i = 0; i < actions.size(); ++i) {
        QJsonObject log;
        log.insert(QStringLiteral("id"), chargerId * 100 + i);
        log.insert(QStringLiteral("charger_id"), chargerId);
        log.insert(QStringLiteral("action"), actions[i]);
        log.insert(QStringLiteral("operator"), QStringLiteral("admin"));
        log.insert(QStringLiteral("op_time"), dateOffset(i));
        log.insert(QStringLiteral("result"), QStringLiteral("success"));
        logs.append(log);
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("logs"), logs);
    return okPayload(payload);
}

QJsonObject MockDataProvider::healthRanks() {
    QJsonArray ranks;
    QVector<QPair<int, int>> scores;  // health, id
    const QJsonArray chargers = chargerCache();
    for (const QJsonValue& cv : chargers) {
        const QJsonObject c = cv.toObject();
        scores.append({c.value(QStringLiteral("health_score")).toInt(),
                       c.value(QStringLiteral("id")).toInt()});
    }
    std::sort(scores.begin(), scores.end());
    for (int i = 0; i < qMin(5, scores.size()); ++i) {
        const int health = scores[i].first;
        QJsonObject r;
        r.insert(QStringLiteral("charger_id"), scores[i].second);
        r.insert(QStringLiteral("health_score"), health);
        r.insert(QStringLiteral("risk_level"), health < 60 ? QStringLiteral("高")
                        : (health < 80 ? QStringLiteral("中") : QStringLiteral("低")));
        r.insert(QStringLiteral("suggestion"), QStringLiteral("建议安排检修 / 远程重启"));
        ranks.append(r);
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("risks"), ranks);
    return okPayload(payload);
}
