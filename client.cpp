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

    m_profilePanel = nullptr;
    m_cartMenu = nullptr;
    m_cartPopup = nullptr;

    installEventFilter(this);
    setupActions();
    createCartMenu();

    // اتصال دکمه نمایش رستوران‌ها به اسلات مربوطه
    connect(ui->showRestaurantsButton, &QPushButton::clicked, this, &Client::on_showRestaurantsButton_clicked);

    // یک درخواست اولیه برای گرفتن داده‌های جدید به سرور می‌فرستیم
    QJsonObject request;
    request["command"] = "get_restaurants";
    NetworkManager::getInstance()->sendJson(request);
}

Client::~Client()
{
    delete ui;
}



void Client::setupActions()
{
    connect(ui->actionProfile, &QAction::triggered, this, &Client::on_actionProfile_triggered);

    NetworkManager* netManager = NetworkManager::getInstance();
    connect(netManager, &NetworkManager::restaurantsReceived, this, &Client::onRestaurantsReceived);
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

// void Client::populateRestaurantList()
// {
//     while (QLayoutItem* item = m_restaurantListLayout->takeAt(0)) {
//         if(item->widget()) delete item->widget();
//         delete item;
//     }

//     if (!m_dbHandler) return;

//     QSqlQuery query = m_dbHandler->getAllRestaurants(m_typeFilterCombo->currentText(), m_locationFilterCombo->currentText(), m_searchLineEdit->text());
//     while (query.next()) {
//         Restaurant r;
//         r.id = query.value("id").toInt();
//         r.name = QString::fromUtf8(query.value("name").toByteArray());
//         r.type = QString::fromUtf8(query.value("type").toByteArray());
//         r.location = QString::fromUtf8(query.value("location").toByteArray());
//         r.priceRange = query.value("price_range").toInt();

//         RestaurantItemWidget *widget = new RestaurantItemWidget();
//         widget->setRestaurantData(r);
//         connect(widget, &RestaurantItemWidget::clicked, this, &Client::onRestaurantClicked);
//         m_restaurantListLayout->addWidget(widget);
//     }
//     m_restaurantListLayout->addStretch();
// }



// void Client::onRestaurantClicked(int restaurantId, const QString& restaurantName)
// {
//     MenuDialog menuDialog(restaurantId, restaurantName, m_dbHandler, this);
//     connect(&menuDialog, &MenuDialog::itemAddedToCart, this, [this](const QJsonObject& foodData, int quantity) {
//         ShoppingCart::getInstance()->addItem(foodData, quantity);
//         this->statusBar()->showMessage(QString("%1 عدد %2 به سبد خرید اضافه شد.").arg(quantity).arg(foodData["name"].toString()), 3000);
//     });
//     menuDialog.exec();
// }

void Client::on_actionProfile_triggered()
{
    if (m_profilePanel) {
        m_profilePanel->activateWindow();
        m_profilePanel->raise();
        return;
    }
    m_profilePanel = new ProfilePanel(m_dbHandler, nullptr);
    m_profilePanel->setAttribute(Qt::WA_DeleteOnClose);
    m_profilePanel->setWindowFlags(Qt::Dialog);
    m_profilePanel->setWindowTitle("پروفایل کاربری");
    connect(this, &Client::historyChanged, m_profilePanel, &ProfilePanel::refreshHistory);
    connect(this, &Client::newChatMessage, m_profilePanel, &ProfilePanel::displayNewMessage);
    connect(m_profilePanel, &QWidget::destroyed, [this]() { m_profilePanel = nullptr; });
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
    if (checkoutDialog.exec() == QDialog::Accepted) {
        QJsonObject orderRequest;
        orderRequest["command"] = "place_order";
        // ... (منطق ساخت JSON برای سفارش) ...
        NetworkManager::getInstance()->sendJson(orderRequest);
        QMessageBox::information(this, "در حال ثبت", "سفارش شما برای سرور ارسال شد.");
        cart->clearCart();
        m_cartPopup->updateContent();
    } else {
        QMessageBox::information(this, "لغو شد", "ثبت سفارش لغو شد.");
    }
}

// <<< پیاده‌سازی اسلات‌های فراموش شده شبکه >>>
// void Client::onRestaurantsReceived(const QJsonArray& restaurantsData)
// {
//     m_dbHandler->clearRestaurantsTable();
//     for (const QJsonValue &value : restaurantsData) {
//         m_dbHandler->addRestaurant(value.toObject());
//     }
//     RestaurantListDialog::populateRestaurantList();
// }

void Client::onOrderStatusUpdated(const QJsonObject& orderData)
{
    qDebug() << "Order status updated for order ID:" << orderData["id"].toInt();
    // m_dbHandler->updateOrder(orderData); // باید این تابع را بسازید
    emit historyChanged();
    QMessageBox::information(this, "به‌روزرسانی سفارش", "وضعیت یکی از سفارش‌های شما تغییر کرد.");
}

void Client::onNewChatMessage(const QJsonObject& chatData)
{
    int orderId = chatData["order_id"].toInt();
    QString sender = chatData["sender"].toString();
    QString message = chatData["message_text"].toString();
    emit newChatMessage(orderId, sender, message);
}
// client.cpp

void Client::on_showRestaurantsButton_clicked()
{
    qDebug() << "Show Restaurants button clicked!";

    // ۱. خواندن مقادیر فعلی از فیلترها در UI
    QString nameFilter = ui->searchLineEdit->text();
    QString typeFilter = ui->typeFilterCombo->currentText();
    QString locationFilter = ui->locationFilterCombo->currentText();

    // ۲. ساخت و اجرای دیالوگ نتایج با پاس دادن فیلترها
    RestaurantListDialog resultsDialog(nameFilter, typeFilter, locationFilter, m_dbHandler, this);

    // ۳. اتصال سیگنال افزودن به سبد خرید از دیالوگ نتایج به سبد خرید مرکزی
    connect(&resultsDialog, &RestaurantListDialog::itemAddedToCart, this,
            [this](const QJsonObject& foodData, int quantity) {

                ShoppingCart::getInstance()->addItem(foodData, quantity);
                this->statusBar()->showMessage(QString("%1 عدد %2 به سبد خرید اضافه شد.")
                                                   .arg(quantity)
                                                   .arg(foodData["name"].toString()), 3000);
            });

    resultsDialog.exec(); // نمایش دیالوگ
}
void Client::onRestaurantsReceived(const QJsonArray& restaurantsData)
{
    qDebug() << "Received restaurant list from server. Updating local cache...";
    m_dbHandler->clearRestaurantsTable();
    for (const QJsonValue &value : restaurantsData) {
        m_dbHandler->addRestaurant(value.toObject());
    }
    // نیازی به رفرش کردن UI نیست، چون کاربر خودش با کلیک روی دکمه نتایج را می‌بیند
}
