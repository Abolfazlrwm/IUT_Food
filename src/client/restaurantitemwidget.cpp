#include "restaurantitemwidget.h"
#include "ui_restaurantitemwidget.h"

RestaurantItemWidget::RestaurantItemWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::RestaurantItemWidget)
{
    ui->setupUi(this);
    setAutoFillBackground(true);
    ui->typeLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));
    ui->locationLabel->setStyleSheet(QStringLiteral("color: #0078d7; font-size: 11px;"));
}

RestaurantItemWidget::~RestaurantItemWidget()
{
    delete ui;
}

void RestaurantItemWidget::setRestaurantData(const RestaurantData &restaurant)
{
    m_restaurantId = restaurant.id;
    ui->nameLabel->setText(restaurant.name);
    ui->typeLabel->setText(restaurant.type);
    ui->locationLabel->setText(restaurant.location);
}
