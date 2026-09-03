#pragma once
// =====================================================================
// 协议草稿（admin 本地暂存版）
// 权威定义：docs/content/spec-协议.md；common/ 定稿后迁移到 common/ 统一引用。
// 新增/修改消息必须先改 spec-协议.md，再通知全组，禁止只改一处。
// =====================================================================
#include <QString>

namespace proto {

namespace field {
const QString kType    = QStringLiteral("type");
const QString kSeq     = QStringLiteral("seq");
const QString kCode    = QStringLiteral("code");
const QString kMessage = QStringLiteral("message");
const QString kPayload = QStringLiteral("payload");
}  // namespace field

namespace type {
// 系统
const QString kSystemPing = QStringLiteral("system.ping");
const QString kSystemPong = QStringLiteral("system.pong");

// ---- 管理端 ----
const QString kAdminLogin          = QStringLiteral("admin.login");
const QString kAdminLoginResp      = QStringLiteral("admin.login_resp");
const QString kAdminRevenue        = QStringLiteral("admin.revenue");
const QString kAdminRevenueResp    = QStringLiteral("admin.revenue_resp");
const QString kAdminStationStatus  = QStringLiteral("admin.station_status");
const QString kAdminStationStatusResp = QStringLiteral("admin.station_status_resp");
const QString kAdminStationList    = QStringLiteral("admin.station_list");
const QString kAdminStationListResp= QStringLiteral("admin.station_list_resp");
const QString kAdminStationDetail  = QStringLiteral("admin.station_detail");
const QString kAdminStationDetailResp = QStringLiteral("admin.station_detail_resp");
const QString kAdminStationAdd     = QStringLiteral("admin.station_add");
const QString kAdminStationAddResp = QStringLiteral("admin.station_add_resp");
const QString kAdminChargerList    = QStringLiteral("admin.charger_list");
const QString kAdminChargerListResp= QStringLiteral("admin.charger_list_resp");
const QString kAdminChargerRestart = QStringLiteral("admin.charger_restart");
const QString kAdminChargerRestartResp = QStringLiteral("admin.charger_restart_resp");
const QString kAdminChargerPause   = QStringLiteral("admin.charger_pause");
const QString kAdminChargerPauseResp = QStringLiteral("admin.charger_pause_resp");
const QString kAdminUserList       = QStringLiteral("admin.user_list");
const QString kAdminUserListResp   = QStringLiteral("admin.user_list_resp");
const QString kAdminUserToggleStatus = QStringLiteral("admin.user_toggle_status");
const QString kAdminUserToggleStatusResp = QStringLiteral("admin.user_toggle_status_resp");
const QString kAdminDeviceLog      = QStringLiteral("admin.device_log");
const QString kAdminDeviceLogResp  = QStringLiteral("admin.device_log_resp");
const QString kAdminFaultRisk      = QStringLiteral("admin.fault_risk");
const QString kAdminFaultRiskResp  = QStringLiteral("admin.fault_risk_resp");
const QString kAdminCockpit        = QStringLiteral("admin.cockpit");
const QString kAdminCockpitResp    = QStringLiteral("admin.cockpit_resp");
const QString kAdminStationAnalysis = QStringLiteral("admin.station_analysis");
const QString kAdminStationAnalysisResp = QStringLiteral("admin.station_analysis_resp");
const QString kAdminAlarmList      = QStringLiteral("admin.alarm_list");
const QString kAdminAlarmListResp  = QStringLiteral("admin.alarm_list_resp");
const QString kAdminAlarmHandle    = QStringLiteral("admin.alarm_handle");
const QString kAdminAlarmHandleResp= QStringLiteral("admin.alarm_handle_resp");
const QString kAdminWorkOrderList  = QStringLiteral("admin.work_order_list");
const QString kAdminWorkOrderListResp = QStringLiteral("admin.work_order_list_resp");
const QString kAdminWorkOrderHandle = QStringLiteral("admin.work_order_handle");
const QString kAdminWorkOrderHandleResp = QStringLiteral("admin.work_order_handle_resp");
const QString kAdminMarketingList  = QStringLiteral("admin.marketing_list");
const QString kAdminMarketingListResp = QStringLiteral("admin.marketing_list_resp");
const QString kAdminCouponCreate   = QStringLiteral("admin.coupon_create");
const QString kAdminCouponCreateResp = QStringLiteral("admin.coupon_create_resp");
const QString kAdminUserPortrait   = QStringLiteral("admin.user_portrait");
const QString kAdminUserPortraitResp = QStringLiteral("admin.user_portrait_resp");
const QString kAdminWhatif         = QStringLiteral("admin.whatif");
const QString kAdminWhatifResp     = QStringLiteral("admin.whatif_resp");
const QString kAdminAssistantQuery = QStringLiteral("admin.assistant_query");
const QString kAdminAssistantQueryResp = QStringLiteral("admin.assistant_query_resp");

// ---- 服务端推送（订阅广播）----
const QString kPushChargerStatus   = QStringLiteral("push.charger_status");
const QString kPushOrderProgress   = QStringLiteral("push.order_progress");
const QString kPushAlarm           = QStringLiteral("push.alarm");
const QString kPushDeviceLog       = QStringLiteral("push.device_log");
const QString kPushOrderEvent      = QStringLiteral("push.order_event");
const QString kPushReservationNotify = QStringLiteral("push.reservation_notify");
const QString kPushWorkOrder       = QStringLiteral("push.work_order");
const QString kPushReview          = QStringLiteral("push.review");
const QString kPushForecast        = QStringLiteral("push.forecast");
}  // namespace type

namespace code {
enum ErrorCode {
    Ok = 0,
    // 认证
    AccountOrPasswordError = 1001,
    PhoneFormatError       = 1002,
    UserFrozen             = 1003,
    // 订单
    OrderUnfinished        = 2001,
    BalanceNotEnough       = 2002,
    OrderStatusNotAllowed  = 2003,
    // 设备
    DeviceOffline          = 3001,
    ChargerNotIdle         = 3002,
    RestartFailed          = 3003,
    // 数据
    DataNotFound           = 4001,
    DataConflict           = 4002,
    // 模型
    ModelUnavailable       = 5001,
    // 协议
    BadMessageFormat       = 9001,
    UnknownMessageType     = 9002,
};
}  // namespace code

}  // namespace proto
