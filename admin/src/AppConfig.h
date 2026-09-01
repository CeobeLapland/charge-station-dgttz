#pragma once
#include <QString>

namespace appconfig {
// 服务端地址（[待定] 建议初值，见 docs/content/spec-协议.md）
const QString kServerUrl = QStringLiteral("ws://127.0.0.1:9000");
constexpr int kHeartbeatIntervalMs = 15 * 1000;
constexpr int kReconnectIntervalMs = 3 * 1000;
constexpr int kHeartbeatTimeoutCount = 3;
const QString kDefaultAccount = QStringLiteral("admin");
const QString kDefaultPassword = QStringLiteral("123456");
}  // namespace appconfig
