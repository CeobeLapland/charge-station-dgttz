#pragma once
#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class ApiClient;

// 运营决策：AI 运营助手（Mock 规则问答）
class DecisionPage : public QWidget {
    Q_OBJECT
public:
    explicit DecisionPage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void onAsk();

private:
    ApiClient* m_api = nullptr;
    QLineEdit* m_questionEdit = nullptr;
    QPlainTextEdit* m_outputView = nullptr;
};