#include "menudialog.h"
#include "ui_menudialog.h"
#include "databasehandler.h"
#include <QSqlQuery>
#include <QVariant>

MenuDialog::MenuDialog(int restaurantId, DataBaseHandler *dbHandler, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MenuDialog),
    m_dbHandler(dbHandler),
    m_restaurantId(restaurantId)
{
    ui->setupUi(this);
    setWindowTitle("منوی رستوران");
    populateMenuItems();
}

MenuDialog::~MenuDialog()
{
    delete ui;
}

#include "menuitemwidget.h" // <<< این را اضافه کنید
#include <QListWidgetItem> // <<< این را اضافه کنید

void MenuDialog::populateMenuItems()
{
    // ...
    QSqlQuery query = m_dbHandler->getMenuItemsForRestaurant(m_restaurantId);
    while (query.next()) {
        // ساخت یک شیء JSON از اطلاعات هر غذا
        QJsonObject foodData;
        foodData["id"] = query.value("id").toInt();
        foodData["name"] = query.value("name").toString();
        foodData["price"] = query.value("price").toDouble();
        // ... سایر اطلاعات ...

        // ساخت آیتم لیست و ویجت سفارشی
        QListWidgetItem *item = new QListWidgetItem(ui->menuListWidget);
        MenuItemWidget *widget = new MenuItemWidget(foodData);

        // اتصال سیگنال ویجت سفارشی به سیگنال دیالوگ
        connect(widget, &MenuItemWidget::addToCartRequested, this, &MenuDialog::itemAddedToCart);

        item->setSizeHint(widget->sizeHint());
        ui->menuListWidget->setItemWidget(item, widget);
    }
}
