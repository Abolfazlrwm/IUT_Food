#include "client.h"
#include "ui_client.h"

#include "databasehandler.h"
#include "checkoutdialog.h"
#include "menudialog.h"
#include "networkmanager.h"
#include "orderstatus.h"
#include "profilepanel.h"
#include "restaurantlistdialog.h"
#include "shoppingcart.h"
#include "shoppingcartpopup.h"

#include <QDebug>
#include <QEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QToolButton>
#include <QWidgetAction>

Client::Client(int userId, const QString &username,
               DataBaseHandler *dbHandler, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::Client),
      m_dbHandler(dbHandler), m_userId(userId), m_username(username)
{
    ui->setupUi(this);
    ui->welcomeLabel->setText(QStringLiteral("سلام %1! چه چیزی میل دارید؟").arg(m_username));

    setupActions();
    createCartMenu();

    // Live connection feedback in the status bar.
    NetworkManager &net = NetworkManager::instance();
    net.connectToServer();
    connect(&net, &NetworkManager::connected, this, &Client::onNetworkStatusChanged);
    connect(&net, &NetworkManager::disconnected, this, &Client::onNetworkStatusChanged);
    connect(&net, &NetworkManager::connectionError, this, [this](const QString &message) {
        statusBar()->showMessage(QStringLiteral("اتصال به سرور برقرار نشد: %1").arg(message), 4000);
    });

    // Push notifications from the server (restaurant confirms/ships an order).
    connect(&net, &NetworkManager::orderStatusUpdated, this, &Client::onOrderStatusUpdated);
    connect(&net, &NetworkManager::chatMessageReceived, this, &Client::onNewChatMessage);
    onNetworkStatusChanged();
}

Client::~Client()
{
    delete ui;
}

void Client::setupActions()
{
    connect(ui->actionProfile, &QAction::triggered, this, &Client::on_actionProfile_triggered);
    connect(ui->showRestaurantsButton, &QPushButton::clicked,
            this, &Client::on_showRestaurantsButton_clicked);
}

void Client::createCartMenu()
{
    m_cartMenu = new QMenu(this);
    m_cartPopup = new ShoppingCartPopup(this);
    auto *widgetAction = new QWidgetAction(this);
    widgetAction->setDefaultWidget(m_cartPopup);
    m_cartMenu->addAction(widgetAction);
    ui->actionCart->setMenu(m_cartMenu);

    auto *cartButton = qobject_cast<QToolButton *>(ui->toolBar->widgetForAction(ui->actionCart));
    if (cartButton)
        cartButton->setPopupMode(QToolButton::InstantPopup);

    connect(m_cartMenu, &QMenu::aboutToShow, this, &Client::onCartMenuAboutToShow);
    connect(m_cartPopup, &ShoppingCartPopup::checkoutRequested, this, &Client::onShowCheckoutDialog);
}

void Client::on_actionProfile_triggered()
{
    if (m_profilePanel) {
        m_profilePanel->raise();
        m_profilePanel->activateWindow();
        return;
    }
    m_profilePanel = new ProfilePanel(m_dbHandler, m_userId, m_username);
    m_profilePanel->setAttribute(Qt::WA_DeleteOnClose);
    m_profilePanel->setWindowFlags(Qt::Dialog);
    m_profilePanel->setWindowTitle(QStringLiteral("پروفایل کاربری"));

    connect(this, &Client::historyChanged, m_profilePanel, &ProfilePanel::refreshHistory);
    connect(m_profilePanel, &ProfilePanel::newChatMessage,
            this, &Client::newChatMessage);
    connect(m_profilePanel, &QObject::destroyed, this, [this]() {
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

void Client::onShowCheckoutDialog()
{
    if (m_cartMenu && m_cartMenu->isVisible())
        m_cartMenu->hide();

    ShoppingCart *cart = ShoppingCart::getInstance();
    if (cart->getItems().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("سبد خالی"),
                             QStringLiteral("سبد خرید شما خالی است!"));
        return;
    }

    CheckoutDialog checkoutDialog(cart, this);
    if (checkoutDialog.exec() != QDialog::Accepted)
        return;

    // Build the order payload.  The database assigns the real id, so no
    // timestamp-derived id collisions can happen anymore.
    QJsonObject orderData;
    orderData[QLatin1String("customer_id")] = m_userId;
    orderData[QLatin1String("restaurant_id")] =
        cart->getItems().first().foodData.value(QLatin1String("restaurant_id")).toInt();
    orderData[QLatin1String("status")] = QString::fromLatin1("pending");
    orderData[QLatin1String("total_price")] = cart->getTotalPrice();
    orderData[QLatin1String("created_at")] =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));

    const int newOrderId = m_dbHandler->createNewOrder(orderData);
    if (newOrderId <= 0) {
        QMessageBox::critical(this, QStringLiteral("خطا"),
                              QStringLiteral("ثبت سفارش در پایگاه داده ناموفق بود."));
        return;
    }

    if (!m_dbHandler->addOrderItems(newOrderId, cart->getItems())) {
        QMessageBox::critical(this, QStringLiteral("خطا"),
                              QStringLiteral("ذخیره اقلام سفارش ناموفق بود."));
        return;
    }

    // Best-effort push to the server so remote dashboards see the order too.
    if (NetworkManager::instance().isConnected()) {
        QJsonObject request = orderData;
        request[QLatin1String("command")] = QLatin1String("place_order");
        QJsonArray items;
        for (const CartItem &item : cart->getItems()) {
            QJsonObject obj = item.foodData;
            obj[QLatin1String("quantity")] = item.quantity;
            items.append(obj);
        }
        request[QLatin1String("items")] = items;
        NetworkManager::instance().sendJson(request);
    }

    // The old code forgot this - the cart stayed full after paying.
    cart->clearCart();
    m_cartPopup->updateContent();

    statusBar()->showMessage(
        QStringLiteral("سفارش شما با شماره %1 ثبت شد ✅").arg(newOrderId), 6000);
    emit historyChanged();
}

void Client::onNetworkStatusChanged()
{
    if (NetworkManager::instance().isConnected())
        statusBar()->showMessage(QStringLiteral("متصل به سرور 🟢"), 4000);
    else
        statusBar()->showMessage(QStringLiteral("حالت آفلاین — سفارش‌ها به‌صورت محلی ثبت می‌شوند"), 6000);
}

void Client::onOrderStatusUpdated(const QJsonObject &orderData)
{
    const int orderId = orderData.value(QLatin1String("order_id")).toInt();
    const QString newStatus = orderData.value(QLatin1String("new_status")).toString();
    if (orderId <= 0 || newStatus.isEmpty())
        return;

    m_dbHandler->updateOrderStatus(orderId, newStatus);
    emit historyChanged();

    QMessageBox::information(this, QStringLiteral("به‌روزرسانی سفارش"),
                             QStringLiteral("وضعیت سفارش شماره %1 به «%2» تغییر کرد.")
                                 .arg(orderId)
                                 .arg(OrderStatus::toPersian(newStatus)));
}

void Client::onNewChatMessage(const QJsonObject &chatData)
{
    const int orderId = chatData.value(QLatin1String("order_id")).toInt();
    const QString sender = chatData.value(QLatin1String("sender")).toString();
    const QString message = chatData.value(QLatin1String("message_text")).toString();
    emit newChatMessage(orderId, sender, message);
}

void Client::on_showRestaurantsButton_clicked()
{
    const QString nameFilter = ui->searchLineEdit->text().trimmed();
    const QString typeFilter = ui->typeFilterCombo->currentText();
    const QString locationFilter = ui->locationFilterCombo->currentText();

    RestaurantListDialog resultsDialog(nameFilter, typeFilter, locationFilter,
                                       m_dbHandler, this);
    connect(&resultsDialog, &RestaurantListDialog::itemAddedToCart, this,
            [this](const QJsonObject &foodData, int quantity) {
                ShoppingCart::getInstance()->addItem(foodData, quantity);
                statusBar()->showMessage(
                    QStringLiteral("%1 عدد %2 به سبد اضافه شد.")
                        .arg(quantity)
                        .arg(foodData.value(QLatin1String("name")).toString()), 3000);
            });
    resultsDialog.exec();
}
