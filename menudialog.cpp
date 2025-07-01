#include "menudialog.h"
#include "ui_menudialog.h"
#include "databasehandler.h"
#include "menuitemwidget.h"  // برای ویجت سفارشی هر آیتم غذا
#include <QSqlQuery>
#include <QVariant>
#include <QListWidgetItem>
#include <QDialogButtonBox>  // برای اتصال دکمه‌های استاندارد

MenuDialog::MenuDialog(int restaurantId, const QString& restaurantName, DataBaseHandler *dbHandler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MenuDialog),
    m_dbHandler(dbHandler),
    m_restaurantId(restaurantId)
{
    ui->setupUi(this);
    setWindowTitle("منوی رستوران");

    // ۱. نمایش نام رستوران در لیبل بالای صفحه
    ui->restaurantNameLabel->setText(QString("منوی رستوران %1").arg(restaurantName));

    // ۲. اتصال دکمه‌های استاندارد (OK/Close) به اسلات‌های داخلی دیالوگ
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ۳. پر کردن لیست غذاها از دیتابیس
    populateMenuItems();
}

MenuDialog::~MenuDialog()
{
    delete ui;
}

void MenuDialog::populateMenuItems()
{
    ui->menuListWidget->clear();
    if (!m_dbHandler || m_restaurantId <= 0) return;

    QSqlQuery query = m_dbHandler->getMenuItemsForRestaurant(m_restaurantId);
    while (query.next()) {
        // ساخت یک شیء JSON کامل از اطلاعات هر غذا
        QJsonObject foodData;
        foodData["id"] = query.value("id").toInt();
        foodData["name"] = query.value("name").toString();
        foodData["description"] = query.value("description").toString();
        foodData["price"] = query.value("price").toDouble();
        foodData["category"] = query.value("category").toString();
        foodData["restaurant_id"] = m_restaurantId;

        // ساخت آیتم لیست و ویجت سفارشی
        QListWidgetItem *item = new QListWidgetItem(ui->menuListWidget);
        MenuItemWidget *widget = new MenuItemWidget(foodData, this);

        // اتصال سیگنال افزودن به سبد خرید از ویجت آیتم به سیگنال این دیالوگ
        connect(widget, &MenuItemWidget::addToCartRequested, this, &MenuDialog::itemAddedToCart);

        item->setSizeHint(widget->sizeHint());
        ui->menuListWidget->setItemWidget(item, widget);
    }
}
