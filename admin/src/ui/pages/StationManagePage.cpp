#include "ui/pages/StationManagePage.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "network/Protocol.h"
#include <QUrl>

#ifdef HAS_WEBENGINE
#include <QtWebEngineWidgets/QWebEngineView>
#endif
#include "services/ApiClient.h"

StationManagePage::StationManagePage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("充电站管理"));
    title->setObjectName(QStringLiteral("pageTitle"));

    auto* addBtn = new QPushButton(QStringLiteral("新增电站"));
    addBtn->setObjectName(QStringLiteral("primaryButton"));
    auto* detailBtn = new QPushButton(QStringLiteral("查看站内详情"));
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"));
    auto* pauseBtn = new QPushButton(QStringLiteral("停用电站"));
    auto* resumeBtn = new QPushButton(QStringLiteral("恢复电站"));

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(title);
    topRow->addStretch();
    topRow->addWidget(addBtn);
    topRow->addWidget(pauseBtn);
    topRow->addWidget(resumeBtn);
    topRow->addWidget(detailBtn);
    topRow->addWidget(refreshBtn);

    m_table = new QTableWidget(0, 8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("站名"),
                                        QStringLiteral("地址"), QStringLiteral("经度"),
                                        QStringLiteral("纬度"), QStringLiteral("总桩数"),
                                        QStringLiteral("在线率"), QStringLiteral("状态")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(m_table);

    connect(addBtn, &QPushButton::clicked, this, &StationManagePage::onAddStation);
    connect(detailBtn, &QPushButton::clicked, this, &StationManagePage::onShowDetail);
    connect(refreshBtn, &QPushButton::clicked, this, &StationManagePage::refresh);
    connect(pauseBtn, &QPushButton::clicked, this, &StationManagePage::onPauseStation);
    connect(resumeBtn, &QPushButton::clicked, this, &StationManagePage::onResumeStation);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { onShowDetail(); });

    refresh();
}

void StationManagePage::refresh() {
    m_api->fetchStations([this](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray stations = payload.value(QStringLiteral("stations")).toArray();
        m_table->setRowCount(stations.size());
        for (int i = 0; i < stations.size(); ++i) {
            const QJsonObject s = stations.at(i).toObject();
            auto* idItem = new QTableWidgetItem(QString::number(s.value(QStringLiteral("id")).toInt()));
            idItem->setData(Qt::UserRole, s.value(QStringLiteral("id")).toInt());
            m_table->setItem(i, 0, idItem);
            m_table->setItem(i, 1, new QTableWidgetItem(s.value(QStringLiteral("name")).toString()));
            m_table->setItem(i, 2, new QTableWidgetItem(s.value(QStringLiteral("address")).toString()));
            m_table->setItem(i, 3, new QTableWidgetItem(
                QString::number(s.value(QStringLiteral("longitude")).toDouble(), 'f', 6)));
            m_table->setItem(i, 4, new QTableWidgetItem(
                QString::number(s.value(QStringLiteral("latitude")).toDouble(), 'f', 6)));
            m_table->setItem(i, 5, new QTableWidgetItem(
                QString::number(s.value(QStringLiteral("total_chargers")).toInt())));
            m_table->setItem(i, 6, new QTableWidgetItem(
                QString::number(s.value(QStringLiteral("online_rate")).toDouble(), 'f', 1)
                + QStringLiteral("%")));
            QString st = s.value(QStringLiteral("status")).toString();
            bool frozen = (st == QStringLiteral("frozen"))
                          || (st.isEmpty() && m_frozenStations.contains(s.value(QStringLiteral("id")).toInt()));
            m_table->setItem(i, 7, new QTableWidgetItem(
                frozen ? QStringLiteral("停用") : QStringLiteral("正常")));
        }
    });
}

void StationManagePage::onAddStation() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新增电站"));
    auto* form = new QFormLayout(&dlg);

    auto* nameEdit = new QLineEdit;
    auto* addressEdit = new QLineEdit;
    auto* lngEdit = new QDoubleSpinBox;
    lngEdit->setRange(-180.0, 180.0);
    lngEdit->setDecimals(6);
    lngEdit->setValue(116.30);
    auto* latEdit = new QDoubleSpinBox;
    latEdit->setRange(-90.0, 90.0);
    latEdit->setDecimals(6);
    latEdit->setValue(39.90);
    auto* totalEdit = new QSpinBox;
    totalEdit->setRange(1, 100);
    totalEdit->setValue(10);

    form->addRow(QStringLiteral("站名"), nameEdit);
    form->addRow(QStringLiteral("地址"), addressEdit);
    form->addRow(QStringLiteral("经度"), lngEdit);
    form->addRow(QStringLiteral("纬度"), latEdit);
    form->addRow(QStringLiteral("电桩数"), totalEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    if (nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("站名不能为空"));
        return;
    }
    QJsonObject station;
    station.insert(QStringLiteral("name"), nameEdit->text().trimmed());
    station.insert(QStringLiteral("address"), addressEdit->text().trimmed());
    station.insert(QStringLiteral("longitude"), lngEdit->value());
    station.insert(QStringLiteral("latitude"), latEdit->value());
    station.insert(QStringLiteral("total_chargers"), totalEdit->value());

    m_api->addStation(station, [this](int code, const QString& message, const QJsonObject&) {
        if (code == proto::code::Ok) {
            refresh();
        } else {
            QMessageBox::warning(this, QStringLiteral("新增失败"), message);
        }
    });
}

void StationManagePage::onPauseStation() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个充电站"));
        return;
    }
    const int stationId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    m_api->fetchChargers(stationId,
                         [this, stationId](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray chargers = payload.value(QStringLiteral("chargers")).toArray();
        QList<int> toPause;
        int busy = 0;   // 在用/预约中的桩不能打断
        for (const QJsonValue& cv : chargers) {
            const QJsonObject c = cv.toObject();
            const QString st = c.value(QStringLiteral("status")).toString();
            if (st == QStringLiteral("charging") || st == QStringLiteral("reserved")) {
                ++busy;
            } else if (st == QStringLiteral("idle")) {
                toPause.append(c.value(QStringLiteral("id")).toInt());
            }
            // fault / offline / rebooting：保持原样，不暂停
        }
        if (busy > 0) {
            QMessageBox::warning(this, QStringLiteral("无法停用"),
                                 QStringLiteral("该站有 %1 台桩正在充电/被预约，为避免中断，请先处理再停用。").arg(busy));
            return;
        }
        m_pausedChargers.insert(stationId, toPause);
        for (int cid : toPause) {
            m_api->pauseCharger(cid, nullptr);
        }
        m_api->pauseStation(stationId, [this, stationId](int c2, const QString& message,
                                                         const QJsonObject&) {
            if (c2 == proto::code::Ok) {
                m_frozenStations.insert(stationId);
            }
            QMessageBox::information(this, QStringLiteral("停用电站"),
                                     c2 == proto::code::Ok
                                         ? QStringLiteral("已停用：站内空闲桩已暂停，故障/离线桩保持原样")
                                         : QStringLiteral("失败：%1").arg(message));
            refresh();
        });
    });
}

void StationManagePage::onResumeStation() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个充电站"));
        return;
    }
    const int stationId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    // 只恢复“停用时由我们暂停的空闲桩”，故障/离线桩本来就没动，仍保持原样
    const QList<int> toResume = m_pausedChargers.value(stationId);
    m_pausedChargers.remove(stationId);
    for (int cid : toResume) {
        m_api->resumeCharger(cid, nullptr);
    }
    m_api->resumeStation(stationId, [this, stationId](int c2, const QString& message,
                                                      const QJsonObject&) {
        if (c2 == proto::code::Ok) {
            m_frozenStations.remove(stationId);
        }
        QMessageBox::information(this, QStringLiteral("恢复电站"),
                                 c2 == proto::code::Ok
                                     ? QStringLiteral("恢复电站")
                                     : QStringLiteral("失败：%1").arg(message));
        refresh();
    });
}
void StationManagePage::onShowDetail() {
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中一座充电站"));
        return;
    }
    const int stationId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const QString name = m_table->item(row, 1)->text();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("站内详情 · %1").arg(name));
    dlg.resize(760, 540);
    auto* layout = new QVBoxLayout(&dlg);
    // 顶部：电站位置地图（Linux+QWebEngine 用 Leaflet+Esri；否则占位）
    const double lng = m_table->item(row, 3)->text().toDouble();
    const double lat = m_table->item(row, 4)->text().toDouble();
#ifdef HAS_WEBENGINE
    {
        auto* mapView = new QWebEngineView(&dlg);
        QString html = QStringLiteral(R"(
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<link rel="stylesheet" href="leaflet.css">
<script src="leaflet.js"></script>
<style>html,body,#m{height:100%;margin:0;background:#dde8f0;}</style>
</head>
<body><div id="m"></div>
<script>
var map = L.map('m').setView([__LAT__, __LNG__], 13);
L.tileLayer('https://webrd0{s}.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8&x={x}&y={y}&z={z}', {subdomains: '1234', maxZoom: 18, attribution: 'AutoNavi'}).addTo(map);
L.circleMarker([__LAT__, __LNG__], {radius: 9, color: '#ffffff', weight: 2, fillColor: '#e63946', fillOpacity: 1}).addTo(map).bindPopup('<b>__NAME__</b>').openPopup();
</script>
</body></html>
)");
        html.replace(QStringLiteral("__LAT__"), QString::number(lat, 'f', 6))
            .replace(QStringLiteral("__LNG__"), QString::number(lng, 'f', 6))
            .replace(QStringLiteral("__NAME__"), name);
        mapView->setHtml(html, QUrl(QStringLiteral("qrc:/map/")));
        mapView->setFixedHeight(260);
        layout->addWidget(mapView);
    }
#else
    {
        auto* placeholder = new QLabel(QStringLiteral(
            "地图需在 Linux + QWebEngine 环境显示\n电站：%1（经度 %2，纬度 %3）")
                                           .arg(name)
                                           .arg(lng, 0, 'f', 6)
                                           .arg(lat, 0, 'f', 6),
                                       &dlg);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setFixedHeight(110);
        placeholder->setStyleSheet(QStringLiteral("color:#8a97a5;"));
        layout->addWidget(placeholder);
    }
#endif
    auto* table = new QTableWidget(0, 6);
    table->setHorizontalHeaderLabels({QStringLiteral("编号"), QStringLiteral("类型"),
                                      QStringLiteral("功率(kW)"), QStringLiteral("状态"),
                                      QStringLiteral("温度(℃)"), QStringLiteral("健康度")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table);

    // Mock 模式下回调为同步，可立即展示；真实模式下数据稍后到达，先弹窗。
    m_api->fetchChargers(stationId,
                         [table](int code, const QString&, const QJsonObject& payload) {
        if (code != proto::code::Ok) {
            return;
        }
        const QJsonArray chargers = payload.value(QStringLiteral("chargers")).toArray();
        table->setRowCount(chargers.size());
        for (int i = 0; i < chargers.size(); ++i) {
            const QJsonObject c = chargers.at(i).toObject();
            const QString type = c.value(QStringLiteral("type")).toString();
            auto* codeItem = new QTableWidgetItem(c.value(QStringLiteral("code")).toString());
            codeItem->setData(Qt::UserRole, c.value(QStringLiteral("id")).toInt());
            table->setItem(i, 0, codeItem);
            table->setItem(i, 1, new QTableWidgetItem(
                type == QStringLiteral("fast") ? QStringLiteral("快充") : QStringLiteral("慢充")));
            table->setItem(i, 2, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("power")).toDouble())));
            table->setItem(i, 3, new QTableWidgetItem(c.value(QStringLiteral("status")).toString()));
            table->setItem(i, 4, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("temperature")).toDouble())));
            table->setItem(i, 5, new QTableWidgetItem(
                QString::number(c.value(QStringLiteral("health_score")).toInt())));
        }
    });
        int jumpId = 0;
        connect(table, &QTableWidget::cellDoubleClicked, &dlg,
                [&dlg, table, &jumpId](int r, int) {
            if (r < 0 || r >= table->rowCount()) {
                return;
            }
            jumpId = table->item(r, 0)->data(Qt::UserRole).toInt();
            dlg.accept();
        });
    dlg.exec();
        if (jumpId > 0) {
            emit openChargerRequested(jumpId);
        }
}

