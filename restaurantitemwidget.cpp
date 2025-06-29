#include "restaurantitemwidget.h"
#include "ui_restaurantitemwidget.h" // این فایل توسط Qt از روی .ui شما ساخته می‌شود
#include <QPixmap>

// کانستراکتور: در اینجا UI را از فایل .ui می‌خوانیم و راه‌اندازی می‌کنیم
RestaurantItemWidget::RestaurantItemWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RestaurantItemWidget)
{
    // این خط، تمام QLabelهایی که در Designer کشیدید را می‌سازد
    ui->setupUi(this);
}

// دیستراکتور: حافظه مربوط به UI را آزاد می‌کند
RestaurantItemWidget::~RestaurantItemWidget()
{
    delete ui;
}

// این تابع، داده‌ها را به UI متصل می‌کند
void RestaurantItemWidget::setRestaurantData(const Restaurant &restaurant)
{
    // ۱. نام رستوران را از شیء restaurant گرفته و در nameLabel قرار می‌دهیم
    ui->nameLabel->setText(restaurant.name);

    // ۲. نوع غذا را در typeLabel قرار می‌دهیم
    ui->typeLabel->setText(restaurant.type);

    // ۳. مسیر عکس لوگو را خوانده، به یک عکس قابل نمایش (QPixmap) تبدیل کرده
    //    و آن را در logoLabel نمایش می‌دهیم. همچنین اندازه آن را تنظیم می‌کنیم.
    ui->logoLabel->setPixmap(QPixmap(restaurant.logoPath)
                                 .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
