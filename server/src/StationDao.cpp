#include "StationDao.h"

#include <QSqlQuery>
#include <QVariant>

namespace dao {

QList<StationView> listStations()
{
    // 子查询现算空闲桩数 —— "指标不建表"的落地:
    // 空闲数随桩状态实时变化, 存库必然过期, 所以每次都从 charger 表数出来。
    QList<StationView> list;
    QSqlQuery q;
    q.exec(QStringLiteral(
        "SELECT s.id, s.name, s.address, s.area, s.longitude, s.latitude, "
        "       s.total_chargers, s.service_fee, "
        "       (SELECT COUNT(*) FROM charger c "
        "         WHERE c.station_id = s.id AND c.status = 'idle') AS free_cnt "
        "FROM station s ORDER BY s.id"));
    while (q.next()) {
        StationView v;
        v.id            = q.value(0).toInt();
        v.name          = q.value(1).toString();
        v.address       = q.value(2).toString();
        v.area          = q.value(3).toString();
        v.longitude     = q.value(4).toDouble();
        v.latitude      = q.value(5).toDouble();
        v.totalChargers = q.value(6).toInt();
        v.serviceFee    = q.value(7).toDouble();
        v.freeChargers  = q.value(8).toInt();
        list.append(v);
    }
    return list;
}

}  // namespace dao
