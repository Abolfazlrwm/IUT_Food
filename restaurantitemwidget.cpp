#include "restaurantitemwidget.h"
#include "ui_restaurantitemwidget.h" // این فایل توسط Qt از روی .ui شما ساخته می‌شود
#include <QMouseEvent>
#include <QPixmap>

// کانستراکتور: در اینجا UI را از فایل .ui می‌خوانیم و راه‌اندازی می‌کنیم
RestaurantItemWidget::RestaurantItemWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RestaurantItemWidget)
{
    // این خط، تمام QLabelهایی که در Designer کشیدید را می‌سازد
    ui->setupUi(this);
    this->setMinimumHeight(70);
}

// دیستراکتور: حافظه مربوط به UI را آزاد می‌کند
RestaurantItemWidget::~RestaurantItemWidget()
{
    delete ui;
}

// این تابع، داده‌ها را به UI متصل می‌کند
// در بالای فایل، include های imagecachemanager و QUrl را پاک کنید
// ...
void RestaurantItemWidget::setRestaurantData(const Restaurant &restaurant) {
    m_restaurantId = restaurant.id;
    ui->nameLabel->setText(restaurant.name);
    ui->typeLabel->setText(restaurant.type);
    // خط مربوط به عکس حذف شد
}
#include "ui_restaurantitemwidget.h" // برای دسترسی به ui

void RestaurantItemWidget::mousePressEvent(QMouseEvent *event)
{
    // حالا هم ID و هم نام رستوران (که از لیبل می‌خوانیم) را ارسال می‌کنیم
    emit clicked(m_restaurantId, ui->nameLabel->text());
    QWidget::mousePressEvent(event);
}
