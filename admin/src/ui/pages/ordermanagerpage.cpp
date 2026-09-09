#include "ordermanagerpage.h"
#include "ui_ordermanagerpage.h"

OrderManagerPage::OrderManagerPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OrderManagerPage)
{
    ui->setupUi(this);
}

OrderManagerPage::~OrderManagerPage()
{
    delete ui;
}


