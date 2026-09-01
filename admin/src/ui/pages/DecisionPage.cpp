#include "ui/pages/DecisionPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "services/ApiClient.h"

DecisionPage::DecisionPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("运营决策（增强模块）"));
    title->setObjectName(QStringLiteral("pageTitle"));

    auto* note = new QLabel(QStringLiteral(
        "占位页：AI 运营助手与 what-if 决策仿真。\n"
        "助手目前用规则给出 Mock 回答，联调后接入机器学习子系统的预测结果。"));
    note->setStyleSheet(QStringLiteral("color:#8a97a5;"));

    m_questionEdit = new QLineEdit;
    m_questionEdit->setPlaceholderText(QStringLiteral("例：今天哪个充电站负载最高？"));
    auto* askBtn = new QPushButton(QStringLiteral("提问"));
    askBtn->setObjectName(QStringLiteral("primaryButton"));
    auto* whatifBtn = new QPushButton(QStringLiteral("what-if 仿真（占位）"));

    auto* qRow = new QHBoxLayout;
    qRow->addWidget(m_questionEdit, 1);
    qRow->addWidget(askBtn);
    qRow->addWidget(whatifBtn);

    m_outputView = new QPlainTextEdit;
    m_outputView->setReadOnly(true);
    m_outputView->setPlaceholderText(QStringLiteral("助手回答将显示在这里…"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(note);
    layout->addLayout(qRow);
    layout->addWidget(m_outputView, 1);

    connect(askBtn, &QPushButton::clicked, this, &DecisionPage::onAsk);
    connect(m_questionEdit, &QLineEdit::returnPressed, this, &DecisionPage::onAsk);
    connect(whatifBtn, &QPushButton::clicked, this, &DecisionPage::onWhatif);
}

void DecisionPage::onAsk() {
    const QString question = m_questionEdit->text().trimmed();
    if (question.isEmpty()) {
        return;
    }
    QString answer;
    if (question.contains(QStringLiteral("负载"))) {
        answer = QStringLiteral(
            "【Mock】科技园站当前负载最高，达到 91%。\n"
            "根据未来 2 小时预测，17:00–19:00 预计持续高峰，建议提前引导用户前往东软园区站。");
    } else if (question.contains(QStringLiteral("收入"))
               || question.contains(QStringLiteral("营收"))) {
        answer = QStringLiteral(
            "【Mock】今日营收 1386.5 元，较昨日下降 7.4%，主要集中在 14:00–16:00 时段；"
            "同期科技园站有 3 台快充桩故障，设备可用率下降是主要原因。");
    } else if (question.contains(QStringLiteral("检修"))
               || question.contains(QStringLiteral("健康"))) {
        answer = QStringLiteral(
            "【Mock】A-023 最需要检修：过去 7 天发生 4 次通信异常，"
            "平均功率波动高于其他设备，健康度 67 分。");
    } else {
        answer = QStringLiteral(
            "【Mock】暂未覆盖该问题。联调后将接入机器学习子系统，"
            "回答将引用真实数据库记录与负荷/故障预测结果。");
    }
    m_outputView->appendPlainText(QStringLiteral("问：%1\n答：%2\n").arg(question, answer));
    m_questionEdit->clear();
}

void DecisionPage::onWhatif() {
    QMessageBox::information(
        this, QStringLiteral("what-if 决策仿真"),
        QStringLiteral("占位：输入假设（加/减桩、电价调整、故障规模、客流变化）后，"
                       "推演平均等待时间、峰值利用率、预计日订单、预计日营收。\n"
                       "推演模型与机器学习子系统共用。"));
}
