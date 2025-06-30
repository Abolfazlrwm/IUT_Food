#include "restaurantlistdialog.h"
#include "ui_restaurantlistdialog.h"
#include "databasehandler.h"
#include "restaurantitemwidget.h"
#include "shoppingcart.h"
#include <QStatusBar>
#include "menudialog.h"
#include "datatypes.h"
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <QLabel> // برای خواندن نام از ویجت

RestaurantListDialog::RestaurantListDialog(const QString& name, const QString& type, const QString& location,
                                           DataBaseHandler *dbHandler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RestaurantListDialog),
    m_dbHandler(dbHandler),
    m_nameFilter(name),
    m_typeFilter(type),
    m_locationFilter(location)
{
    ui->setupUi(this);
    setWindowTitle("نتایج جستجو");

    // اتصال سیگنال کلیک از QListWidget به اسلات همین کلاس
    connect(ui->restaurantListWidget, &QListWidget::itemClicked, this, &RestaurantListDialog::onRestaurantItemClicked);

    // پر کردن لیست با نتایج
    populateRestaurantList();
}

RestaurantListDialog::~RestaurantListDialog()
{
    delete ui;
}

void RestaurantListDialog::populateRestaurantList()
{
    ui->restaurantListWidget->clear();
    if (!m_dbHandler) return;

    QSqlQuery query = m_dbHandler->getAllRestaurants(m_typeFilter, m_locationFilter, m_nameFilter);

    while (query.next())
    {
        Restaurant r;
        r.id = query.value("id").toInt();
        r.name = QString::fromUtf8(query.value("name").toByteArray());
        r.type = QString::fromUtf8(query.value("type").toByteArray());
        r.location = QString::fromUtf8(query.value("location").toByteArray());
        r.priceRange = query.value("price_range").toInt();

        // ۱. ساخت آیتم (ردیف) جدید
        QListWidgetItem *item = new QListWidgetItem(ui->restaurantListWidget);
        item->setData(Qt::UserRole, r.id); // ذخیره ID رستوران

        // ۲. ساخت ویجت سفارشی
        RestaurantItemWidget *widget = new RestaurantItemWidget();
        widget->setRestaurantData(r);

        // ۳. تنظیم اندازه و قرار دادن ویجت در آیتم
        item->setSizeHint(widget->sizeHint());
        ui->restaurantListWidget->setItemWidget(item, widget);
    }
}
// restaurantlistdialog.cpp

void RestaurantListDialog::onRestaurantItemClicked(QListWidgetItem *item)
{
    int restaurantId = item->data(Qt::UserRole).toInt();
    RestaurantItemWidget* widget = qobject_cast<RestaurantItemWidget*>(ui->restaurantListWidget->itemWidget(item));
    QString restaurantName = "";
    if (widget) {
        restaurantName = widget->findChild<QLabel*>("nameLabel")->text();
    }

    if (restaurantId > 0) {
        MenuDialog menuDialog(restaurantId, restaurantName, m_dbHandler, this);

        // === اتصال کلیدی در اینجا اتفاق می‌افتد ===
        // وقتی منو خبر می‌دهد که آیتمی اضافه شده، ما هم همان خبر را به بیرون می‌دهیم
        connect(&menuDialog, &MenuDialog::itemAddedToCart,
                this, &RestaurantListDialog::itemAddedToCart);
        // ==========================================

        menuDialog.exec();
    }
}
