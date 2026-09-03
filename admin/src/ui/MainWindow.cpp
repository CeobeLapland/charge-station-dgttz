#include "ui/MainWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "services/ApiClient.h"
#include "ui/pages/ChargerManagePage.h"
#include "ui/pages/ChargerStatusPage.h"
#include "ui/pages/DecisionPage.h"
#include "ui/pages/DeviceOpsPage.h"
#include "ui/pages/SalesPage.h"
#include "ui/pages/StationManagePage.h"
#include "ui/pages/UserManagePage.h"

MainWindow::MainWindow(ApiClient* api, const QString& account, QWidget* parent)
    : QMainWindow(parent), m_api(api), m_account(account) {
    setWindowTitle(QStringLiteral("服务器管理端 · 运营管理后台"));
    resize(1280, 800);

    auto* central = new QWidget;
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ===== 左侧导航 =====
    m_navList = new QListWidget;
    m_navList->setObjectName(QStringLiteral("navList"));
    m_navList->setFixedWidth(200);
    m_navList->addItem(QStringLiteral("销售业绩"));
    m_navList->addItem(QStringLiteral("电桩状态"));
    m_navList->addItem(QStringLiteral("充电桩管理"));
    m_navList->addItem(QStringLiteral("充电站管理"));
    m_navList->addItem(QStringLiteral("用户管理"));
    m_navList->addItem(QStringLiteral("设备运维"));
    m_navList->addItem(QStringLiteral("运营决策"));

    // ===== 右侧 =====
    auto* right = new QWidget;
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // 顶部栏
    auto* topBar = new QWidget;
    topBar->setObjectName(QStringLiteral("topBar"));
    topBar->setFixedHeight(52);
    auto* topLayout = new QHBoxLayout(topBar);
    auto* appName = new QLabel(QStringLiteral("电动汽车充电桩应用管理平台 · 服务器管理端"));
    appName->setStyleSheet(QStringLiteral("font-size:16px;font-weight:bold;color:#f0f4f8;"));
    auto* userLabel = new QLabel(QStringLiteral("当前管理员：%1").arg(m_account));
    auto* logoutBtn = new QPushButton(QStringLiteral("退出登录"));
    topLayout->addWidget(appName);
    topLayout->addStretch();
    topLayout->addWidget(userLabel);
    topLayout->addWidget(logoutBtn);

    // 页面容器
    m_pages = new QStackedWidget;
    m_salesPage = new SalesPage(m_api);
    m_chargerStatusPage = new ChargerStatusPage(m_api);
    m_chargerManagePage = new ChargerManagePage(m_api);
    m_stationManagePage = new StationManagePage(m_api);
    m_userManagePage = new UserManagePage(m_api);
    m_deviceOpsPage = new DeviceOpsPage(m_api);
    m_decisionPage = new DecisionPage(m_api);
    m_pages->addWidget(m_salesPage);
    m_pages->addWidget(m_chargerStatusPage);
    m_pages->addWidget(m_chargerManagePage);
    m_pages->addWidget(m_stationManagePage);
    m_pages->addWidget(m_userManagePage);
    m_pages->addWidget(m_deviceOpsPage);
    m_pages->addWidget(m_decisionPage);

    // 底部状态栏
    auto* statusBar = new QWidget;
    statusBar->setObjectName(QStringLiteral("topBar"));
    statusBar->setFixedHeight(40);
    auto* statusLayout = new QHBoxLayout(statusBar);
    m_statusLabel = new QLabel;
    auto* aiBtn = new QPushButton(QStringLiteral("AI 运营助手"));
    aiBtn->setObjectName(QStringLiteral("primaryButton"));
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(aiBtn);

    rightLayout->addWidget(topBar);
    rightLayout->addWidget(m_pages, 1);
    rightLayout->addWidget(statusBar);

    rootLayout->addWidget(m_navList);
    rootLayout->addWidget(right, 1);
    setCentralWidget(central);

    connect(m_navList, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogout);
    connect(aiBtn, &QPushButton::clicked, this, &MainWindow::onAiAssistant);
    connect(m_api, &ApiClient::connectionStateChanged, this, [this](bool connected) {
        m_statusLabel->setText(connected ? QStringLiteral("● 已连接服务端")
                                         : QStringLiteral("○ Mock 模式（服务端未连接）"));
    });

    m_statusLabel->setText(m_api->isMockMode()
                               ? QStringLiteral("○ Mock 模式（服务端未连接）")
                               : QStringLiteral("● 已连接服务端"));
    m_navList->setCurrentRow(0);
}

void MainWindow::onLogout() {
    emit logoutRequested();
    close();
}

void MainWindow::onAiAssistant() {
    m_navList->setCurrentRow(6);  // 跳转到运营决策页
}
