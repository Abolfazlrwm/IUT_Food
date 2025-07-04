#include "restaurantlistdialog.h"
#include "ui_restaurantlistdialog.h"
#include "databasehandler.h"
#include "networkmanager.h" // <<< برای ارسال درخواست
#include "restaurantmodel.h"
#include "menudialog.h"
#include "restaurantmodel.h"
#include "restaurantdelegate.h"
#include <QSqlQuery>
#include <QDialogButtonBox>
#include <QDebug>

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

    // Server
    QJsonObject req;
    req["command"] = "get_restaurants";
    req["name"] = m_nameFilter;
    req["type"] = m_typeFilter;
    req["location"] = m_locationFilter;
    NetworkManager::getInstance()->sendJson(req);
    connect(NetworkManager::getInstance(), &NetworkManager::restaurantsReceived, this, &RestaurantListDialog::onRestaurantsReceived);



    // ۱. ساخت مدل و نماینده و تنظیم آنها روی QListView
    m_model = new RestaurantModel(this);
    RestaurantDelegate *delegate = new RestaurantDelegate(this);
    ui->restaurantListView->setModel(m_model);
    ui->restaurantListView->setItemDelegate(delegate);

    // ۲. اتصال سیگنال‌های داخلی
    connect(ui->restaurantListView, &QListView::clicked, this, &RestaurantListDialog::onRestaurantItemClicked);
    if (ui->buttonBox) {
        connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    }

    // ۳. ابتدا لیست را با هر داده‌ای که در کش محلی هست، نمایش می‌دهیم
    populateModelFromDb();

    // ۴. به مدیر شبکه متصل می‌شویم تا پاسخ سرور را دریافت کنیم
    connect(NetworkManager::getInstance(), &NetworkManager::restaurantsReceived, this, &RestaurantListDialog::onRestaurantsReceived);

    // ۵. یک درخواست جدید برای گرفتن آخرین لیست رستوران‌ها به سرور می‌فرستیم
    QJsonObject request;
    request["command"] = "get_restaurants";
    NetworkManager::getInstance()->sendJson(request);
}

RestaurantListDialog::~RestaurantListDialog()
{
    delete ui;
}

// این اسلات زمانی اجرا می‌شود که لیست جدید رستوران‌ها از سرور برسد
void RestaurantListDialog::onRestaurantsReceived(const QJsonArray& restaurantsData)
{
    qDebug() << "RestaurantListDialog: Received data. Updating cache and view...";

    // دیتابیس محلی (کش) را آپدیت می‌کنیم
    m_dbHandler->clearRestaurantsTable();
    for (const QJsonValue &value : restaurantsData) {
        m_dbHandler->addRestaurant(value.toObject());
    }

    // حالا که کش آپدیت شد، مدل را دوباره از نو پر می‌کنیم
    populateModelFromDb();
}

// این تابع فقط مسئول خواندن از دیتابیس محلی و پر کردن مدل است
void RestaurantListDialog::populateModelFromDb()
{
    if (!m_dbHandler) return;
    QSqlQuery query = m_dbHandler->getAllRestaurants(m_typeFilter, m_locationFilter, m_nameFilter);
    m_model->populateData(query); // مدل داده‌های جدید را دریافت می‌کند و به صورت خودکار View را آپدیت می‌کند
}

void RestaurantListDialog::onRestaurantItemClicked(const QModelIndex &index)
{
    RestaurantData restaurant = m_model->getRestaurant(index.row());
    if (restaurant.id > 0) {
        MenuDialog menuDialog(restaurant.id, restaurant.name, m_dbHandler, this);
        connect(&menuDialog, &MenuDialog::itemAddedToCart, this, &RestaurantListDialog::itemAddedToCart);
        menuDialog.exec();
    }
}
