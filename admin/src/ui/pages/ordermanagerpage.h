#ifndef ORDERMANAGERPAGE_H
#define ORDERMANAGERPAGE_H

#include <QWidget>

namespace Ui {
class OrderManagerPage;
}

class OrderManagerPage : public QWidget
{
    Q_OBJECT

public:
    explicit OrderManagerPage(QWidget *parent = nullptr);
    ~OrderManagerPage();

private:
    Ui::OrderManagerPage *ui;
};

#endif // ORDERMANAGERPAGE_H

