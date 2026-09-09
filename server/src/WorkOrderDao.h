#pragma once
#include <QList>
#include <QString>
#include <optional>

// ============================================================
// WorkOrderDao — 工单(work_order 表)
// 用户端提交申诉/报修 → work_order.create / work_order.list
// 管理端处理工单 → admin.work_order_list / handle(后续实现)
// ============================================================

struct WorkOrderRow {
    int     id = 0, userId = 0, stationId = 0, chargerId = 0;
    QString type;        // user_complaint / device_fault / refund / maintenance / abnormal_order
    QString priority;    // low / medium / high
    QString title, description, status, handler, result;
    QString createTime, handleTime;
    QString stationName; // JOIN 出来方便前端直接显示
};

namespace dao {

// 提交工单。type 不在枚举内会被纠正为 user_complaint。
std::optional<WorkOrderRow> createWorkOrder(int userId, const QString &type,
                                            int stationId, int chargerId,
                                            const QString &title, const QString &description);

// 我的工单(按时间倒序)。
QList<WorkOrderRow> listWorkOrders(int userId);

// 管理端工单列表。status 为空表示全部。
QList<WorkOrderRow> listAllWorkOrders(const QString &status);

// 单条工单(校验归属)。
std::optional<WorkOrderRow> findWorkOrder(int userId, int workOrderId);

// 管理端处理/回复工单。status 为空时默认 completed。
std::optional<WorkOrderRow> handleWorkOrder(int workOrderId, const QString &handler,
                                            const QString &status, const QString &result);

}  // namespace dao
