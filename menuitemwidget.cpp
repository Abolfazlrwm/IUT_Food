#include "menuitemwidget.h"
#include "ui_menuitemwidget.h"
#include <QVariant>

MenuItemWidget::MenuItemWidget(const QJsonObject& foodData, QWidget *parent) :
    QWidget(parent), ui(new Ui::MenuItemWidget), m_foodData(foodData)
{
    ui->setupUi(this);

    // پر کردن اطلاعات اولیه ویجت
    ui->nameLabel->setText(foodData["name"].toString());
    ui->priceLabel->setText(QString("%1 تومان").arg(foodData["price"].toDouble()));

    // اتصال دکمه افزودن به یک لامبدا که سیگنال اصلی کلاس را منتشر می‌کند
    connect(ui->addButton, &QPushButton::clicked, this, [this](){
        int quantity = ui->quantitySpinBox->value();
        emit addToCartRequested(m_foodData, quantity);
    });
}

MenuItemWidget::~MenuItemWidget()
{
    delete ui;
}
