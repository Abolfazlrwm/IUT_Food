#include "client.h"
#include "ui_client.h"

// هدرهای مورد نیاز
#include "profilepanel.h"
#include "shoppingcartpopup.h"
#include "checkoutdialog.h"
#include "restaurantitemwidget.h"
#include "databasehandler.h"
#include "networkmanager.h"
#include "datatypes.h"
#include "menudialog.h"
#include "shoppingcart.h"
#include "restaurantlistdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <QMessageBox>
#include <QMenu>
#include <QToolButton>
#include <QWidgetAction>
#include <QEvent>
#include <QMouseEvent>
#include <QStatusBar>
Client::Client(DataBaseHandler *dbHandler, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::Client), m_dbHandler(dbHandler)
{
    ui->setupUi(this);

    // مقداردهی اولیه پوینترها
    m_profilePanel = nullptr;
    m_cartMenu = nullptr;
    m_cartPopup = nullptr;

    // راه‌اندازی تمام اتصالات و بخش‌های دیگر
    setupActions();
    createCartMenu();

    // اتصال دکمه نمایش رستوران‌ها به اسلات مربوطه
    connect(ui->showRestaurantsButton, &QPushButton::clicked, this, &Client::on_showRestaurantsButton_clicked);

    // === درخواست آپدیت، فقط یک بار در ابتدای برنامه ===
    qDebug() << "Requesting initial restaurant list from server...";
    QJsonObject request;
    request["command"] = "get_restaurants";
    //NetworkManager::getInstance()->sendJson(request);
}

Client::~Client()
{
    delete ui;
}

void Client::setupActions()
{
    connect(ui->actionProfile, &QAction::triggered, this, &Client::on_actionProfile_triggered);
    connect(ui->showRestaurantsButton, &QPushButton::clicked, this, &Client::on_showRestaurantsButton_clicked);


    NetworkManager* netManager = NetworkManager::getInstance();
    // connect(netManager, &NetworkManager::restaurantsReceived, this, &Client::onRestaurantsReceived);
    connect(netManager, &NetworkManager::orderStatusUpdated, this, &Client::onOrderStatusUpdated);
    connect(netManager, &NetworkManager::newMessageReceived, this, &Client::onNewChatMessage);
}

void Client::createCartMenu()
{
    m_cartMenu = new QMenu(this);
    m_cartPopup = new ShoppingCartPopup(this);
    QWidgetAction *widgetAction = new QWidgetAction(this);
    widgetAction->setDefaultWidget(m_cartPopup);
    m_cartMenu->addAction(widgetAction);
    ui->actionCart->setMenu(m_cartMenu);

    QToolButton *cartButton = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionCart));
    if (cartButton)
        cartButton->setPopupMode(QToolButton::InstantPopup);

    connect(m_cartMenu, &QMenu::aboutToShow, this, &Client::onCartMenuAboutToShow);
    connect(m_cartPopup, &ShoppingCartPopup::checkoutRequested, this, &Client::onShowCheckoutDialog);
}



// client.cpp

void Client::on_actionProfile_triggered()
{
    // اگر پنجره از قبل باز است، فقط آن را فعال کن
    if (m_profilePanel) {
        m_profilePanel->activateWindow();
        m_profilePanel->raise();
        return;
    }

    // یک نمونه جدید بساز
    m_profilePanel = new ProfilePanel(m_dbHandler, nullptr);
    m_profilePanel->setAttribute(Qt::WA_DeleteOnClose);
    m_profilePanel->setWindowFlags(Qt::Dialog);
    m_profilePanel->setWindowTitle("پروفایل کاربری");

    // === اتصال کلیدی برای رفرش شدن خودکار ===
    // هر وقت Client خبر دهد که تاریخچه تغییر کرده، اسلات refreshHistory در ProfilePanel اجرا می‌شود
    connect(this, &Client::historyChanged, m_profilePanel, &ProfilePanel::refreshHistory);
    // ===========================================

    connect(m_profilePanel, &QWidget::destroyed, [this]() {
        m_profilePanel = nullptr;
    });

    m_profilePanel->show();
}

bool Client::eventFilter(QObject *watched, QEvent *event)
{
    return QMainWindow::eventFilter(watched, event);
}

void Client::onCartMenuAboutToShow()
{
    m_cartPopup->updateContent();
}

// client.cpp

// client.cpp

void Client::onShowCheckoutDialog()
{
    if (m_cartMenu && m_cartMenu->isVisible()) {
        m_cartMenu->hide();
    }
    ShoppingCart* cart = ShoppingCart::getInstance();
    if (cart->getItems().isEmpty()) {
        QMessageBox::warning(this, "سبد خالی", "سبد خرید شما خالی است!");
        return;
    }

    CheckoutDialog checkoutDialog(cart, this);
    if (checkoutDialog.exec() == QDialog::Accepted)
    {
        // ساخت اطلاعات سفارش
        QJsonObject orderData;
        int newOrderId = QDateTime::currentMSecsSinceEpoch() % 100000;

        orderData["id"] = newOrderId;
        orderData["customer_id"] = 1; // فرض
        orderData["status"] = "در انتظار تایید";
        orderData["total_price"] = cart->getTotalPrice();
        orderData["created_at"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        orderData["restaurant_id"] = cart->getItems().first().foodData["restaurant_id"].toInt();

        // ۱. ابتدا اطلاعات کلی سفارش را در جدول 'orders' ذخیره می‌کنیم
        if (m_dbHandler->createNewOrder(orderData)) {

            // ۲. سپس آیتم‌های آن را در جدول 'order_items' ذخیره می‌کنیم

            m_dbHandler->addOrderItems(newOrderId, cart->getItems());

            bool success = m_dbHandler->addOrderItems(newOrderId, cart->getItems());
            if (!success) {
                qDebug() << "CRITICAL: Failed to add order items to database!";
            } else {
                qDebug() << "Successfully added order items for order ID:" << newOrderId;
            }

            qDebug() << "New order" << newOrderId << "and its items saved to local DB.";
            emit historyChanged(); // به پنل پروفایل خبر می‌دهیم که تاریخچه آپدیت شده
        }

        // === ارسال سفارش به سرور ===
        QJsonObject req = orderData;
        req["command"] = "place_order";

        QJsonArray items;
        for (const CartItem &item : cart->getItems()) {
            QJsonObject obj = item.foodData;
            obj["quantity"] = item.quantity;
            items.append(obj);
        }
        req["items"] = items;

        NetworkManager::getInstance()->sendJson(req);

    }}
// void Client::onRestaurantsReceived(const QJsonArray& restaurantsData)
// {
//     qDebug() << "Received" << restaurantsData.count() << "restaurants from server. Updating local cache...";

//     // ۱. جدول کش محلی را پاک می‌کنیم
//     m_dbHandler->clearRestaurantsTable();

//     // ۲. داده‌های جدید دریافتی از شبکه را در دیتابیس محلی (کش) ذخیره می‌کنیم
//     for (const QJsonValue &value : restaurantsData) {
//         m_dbHandler->addRestaurant(value.toObject());
//     }

//     statusBar()->showMessage("لیست رستوران‌ها از سرور به‌روز شد!", 4000);
// }

// client.cpp

void Client::onOrderStatusUpdated(const QJsonObject& orderData)
{
    int orderId = orderData["id"].toInt();
    QString newStatus = orderData["new_status"].toString(); // فرض می‌کنیم سرور وضعیت جدید را در این فیلد می‌فرستد

    qDebug() << "Order status update received for order ID:" << orderId << "New status:" << newStatus;

    // <<< استفاده از تابع جدید برای آپدیت دیتابیس محلی >>>
    m_dbHandler->updateOrderStatus(orderId, newStatus);

    // به همه بخش‌های برنامه (مثل ProfilePanel) خبر می‌دهیم که تاریخچه تغییر کرده است
    emit historyChanged();

    QMessageBox::information(this, "به‌روزرسانی سفارش",
                             QString("وضعیت سفارش شماره %1 به '%2' تغییر کرد.").arg(orderId).arg(newStatus));
}


void Client::onNewChatMessage(const QJsonObject& chatData)
{
    int orderId = chatData["order_id"].toInt();
    QString sender = chatData["sender"].toString();
    QString message = chatData["message_text"].toString();
    emit newChatMessage(orderId, sender, message);
}
void Client::on_showRestaurantsButton_clicked()
{
    // مقادیر فیلترها را از UI می‌خوانیم
    QString nameFilter = ui->searchLineEdit->text();
    QString typeFilter = ui->typeFilterCombo->currentText();
    QString locationFilter = ui->locationFilterCombo->currentText();

    // دیالوگ نتایج، همیشه از آخرین داده‌های موجود در دیتابیس محلی می‌خواند
    RestaurantListDialog resultsDialog(nameFilter, typeFilter, locationFilter, m_dbHandler, this);

    // اتصال سیگنال افزودن به سبد خرید
    connect(&resultsDialog, &RestaurantListDialog::itemAddedToCart, this,
            [this](const QJsonObject& foodData, int quantity) {
                ShoppingCart::getInstance()->addItem(foodData, quantity);
                statusBar()->showMessage(QString("%1 عدد %2 اضافه شد.").arg(quantity).arg(foodData["name"].toString()), 3000);
            });

    resultsDialog.exec();
}
