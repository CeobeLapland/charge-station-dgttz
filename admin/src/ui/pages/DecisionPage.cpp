#include "ui/pages/DecisionPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "services/ApiClient.h"

DecisionPage::DecisionPage(ApiClient* api, QWidget* parent)
    : QWidget(parent), m_api(api) {
    auto* title = new QLabel(QStringLiteral("运营决策 · AI 运营助手"));
    title->setObjectName(QStringLiteral("pageTitle"));

    auto* hint = new QLabel(QStringLiteral(
        "可问：哪个站负载最高 / 营收为什么降 / 哪台桩该检修"));
    hint->setStyleSheet(QStringLiteral("color:#8a97a5;"));
    hint->setWordWrap(true);

    m_questionEdit = new QLineEdit;
    m_questionEdit->setPlaceholderText(QStringLiteral("例：今天哪个充电站负载最高？"));
    auto* askBtn = new QPushButton(QStringLiteral("提问"));
    askBtn->setObjectName(QStringLiteral("primaryButton"));
    auto* clearBtn = new QPushButton(QStringLiteral("清空"));

    auto* qRow = new QHBoxLayout;
    qRow->addWidget(m_questionEdit, 1);
    qRow->addWidget(askBtn);
    qRow->addWidget(clearBtn);

    m_outputView = new QPlainTextEdit;
    m_outputView->setReadOnly(true);
    m_outputView->setPlaceholderText(QStringLiteral("助手回答将显示在这里…"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addLayout(qRow);
    layout->addWidget(m_outputView, 1);

    connect(askBtn, &QPushButton::clicked, this, &DecisionPage::onAsk);
    connect(m_questionEdit, &QLineEdit::returnPressed, this, &DecisionPage::onAsk);
    connect(clearBtn, &QPushButton::clicked, m_outputView, &QPlainTextEdit::clear);
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
            "未来 2 小时预测 17:00-19:00 持续高峰，建议提前引导用户前往东软园区站。");
    } else if (question.contains(QStringLiteral("收入")) || question.contains(QStringLiteral("营收"))) {
        answer = QStringLiteral(
            "【Mock】今日营收 1386.5 元，较昨日下降 7.4%，集中在 14:00-16:00 时段；"
            "同期科技园站 3 台快充桩故障，可用率下降是主因。");
    } else if (question.contains(QStringLiteral("检修")) || question.contains(QStringLiteral("健康"))) {
        answer = QStringLiteral(
            "【Mock】A-023 最需要检修：7 天 4 次通信异常、功率波动高，健康度 67 分。");
    } else {
        answer = QStringLiteral(
            "【Mock】暂未覆盖该问题。联调后回答将引用真实数据库记录与负荷/故障预测。");
    }
    m_outputView->appendPlainText(QStringLiteral("问：%1\n答：%2\n").arg(question, answer));
    m_questionEdit->clear();
}
