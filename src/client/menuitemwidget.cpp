#include "menuitemwidget.h"
#include "ui_menuitemwidget.h"

#include <QPushButton>

MenuItemWidget::MenuItemWidget(const QJsonObject &foodData, QWidget *parent)
    : QWidget(parent), ui(new Ui::MenuItemWidget), m_foodData(foodData)
{
    ui->setupUi(this);

    ui->nameLabel->setText(foodData.value(QLatin1String("name")).toString());
    ui->descLabel->setText(foodData.value(QLatin1String("description")).toString());
    ui->descLabel->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    ui->priceLabel->setText(
        QStringLiteral("%1 تومان").arg(foodData.value(QLatin1String("price")).toDouble()));
    ui->priceLabel->setStyleSheet(QStringLiteral("color: #0078d7; font-weight: bold;"));

    connect(ui->addButton, &QPushButton::clicked, this, [this]() {
        emit addToCartRequested(m_foodData, ui->quantitySpinBox->value());
    });
}

MenuItemWidget::~MenuItemWidget()
{
    delete ui;
}
