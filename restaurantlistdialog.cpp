#include "restaurantlistdialog.h"
#include "ui_restaurantlistdialog.h"
#include "databasehandler.h"
#include "menudialog.h"
#include "restaurantmodel.h"  // << اضافه شد
#include "restaurantdelegate.h" // << اضافه شد
#include <QSqlQuery>

RestaurantListDialog::RestaurantListDialog(const QString& name, const QString& type, const QString& location,
                                           DataBaseHandler *dbHandler, QWidget *parent) :
    QDialog(parent), ui(new Ui::RestaurantListDialog), m_dbHandler(dbHandler)
{
    ui->setupUi(this);
    setWindowTitle("نتایج جستجو");

    // ۱. ساخت مدل و نماینده
    m_model = new RestaurantModel(this);
    RestaurantDelegate *delegate = new RestaurantDelegate(this);

    // ۲. تنظیم مدل و نماینده روی QListView
    ui->restaurantListView->setModel(m_model);
    ui->restaurantListView->setItemDelegate(delegate);

    // ۳. گرفتن داده‌ها از دیتابیس و پر کردن مدل
    QSqlQuery query = m_dbHandler->getAllRestaurants(type, location, name);
    m_model->populateData(query);

    // ۴. اتصال سیگنال کلیک
    connect(ui->restaurantListView, &QListView::clicked, this, &RestaurantListDialog::onRestaurantItemClicked);
}

RestaurantListDialog::~RestaurantListDialog() { delete ui; }

void RestaurantListDialog::onRestaurantItemClicked(const QModelIndex &index)
{
    Restaurant restaurant = m_model->getRestaurant(index.row());
    if (restaurant.id > 0) {
        MenuDialog menuDialog(restaurant.id, restaurant.name, m_dbHandler, this);
        connect(&menuDialog, &MenuDialog::itemAddedToCart, this, &RestaurantListDialog::itemAddedToCart);
        menuDialog.exec();
    }
}
