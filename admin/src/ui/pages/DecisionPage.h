#pragma once
#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class ApiClient;

class DecisionPage : public QWidget {
    Q_OBJECT
public:
    explicit DecisionPage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void onAsk();
    void onWhatif();

private:
    ApiClient* m_api = nullptr;
    QLineEdit* m_questionEdit = nullptr;
    QPlainTextEdit* m_outputView = nullptr;
};
