#include "client.h"
#include "ui_client.h"

#include "databasehandler.h"
#include "checkoutdialog.h"
#include "menudialog.h"
#include "networkmanager.h"
#include "orderstatus.h"
#include "ordertrackerdialog.h"
#include "profilepanel.h"
#include "restaurantlistdialog.h"
#include "shoppingcart.h"
#include "shoppingcartpopup.h"
#include "thememanager.h"

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

    // Light/dark switch lives in the toolbar next to profile & cart.
    m_themeAction = ui->toolBar->addAction(
        ThemeManager::isDark() ? QStringLiteral("☀️ تم روشن") : QStringLiteral("🌙 تم تاریک"));
    connect(m_themeAction, &QAction::triggered, this, [this]() {
        ThemeManager::toggle();
        m_themeAction->setText(ThemeManager::isDark()
                                   ? QStringLiteral("☀️ تم روشن")
                                   : QStringLiteral("🌙 تم تاریک"));
    });
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
    connect(m_profilePanel, &ProfilePanel::reorderRequested, this,
            [this](const QJsonObject &foodData, int quantity) {
                ShoppingCart::getInstance()->addItem(foodData, quantity);
                statusBar()->showMessage(
                    QStringLiteral("%1 عدد %2 به سبد اضافه شد (سفارش دوباره).")
                        .arg(quantity)
                        .arg(foodData.value(QLatin1String("name")).toString()), 4000);
            });
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

    CheckoutDialog checkoutDialog(cart, m_userId, m_dbHandler, this);
    if (checkoutDialog.exec() != QDialog::Accepted)
        return;

    const double finalTotal = cart->getTotalPrice() - checkoutDialog.discount();
    const QString paymentMethod = checkoutDialog.paymentMethod();

    // Wallet payment: verify the balance BEFORE creating the order so a
    // failed payment never leaves an orphan row behind.
    if (paymentMethod == QLatin1String("wallet")
        && m_dbHandler->getWalletBalance(m_userId) < finalTotal) {
        QMessageBox::warning(this, QStringLiteral("موجودی کافی نیست"),
                             QStringLiteral("موجودی کیف پول شما کافی نیست.\n"
                                            "از پنل پروفایل می‌توانید کیف پول را شارژ کنید."));
        return;
    }

    // Build the order payload.  The database assigns the real id, so no
    // timestamp-derived id collisions can happen anymore.
    QJsonObject orderData;
    orderData[QLatin1String("customer_id")] = m_userId;
    orderData[QLatin1String("restaurant_id")] =
        cart->getItems().first().foodData.value(QLatin1String("restaurant_id")).toInt();
    orderData[QLatin1String("status")] = QString::fromLatin1("pending");
    orderData[QLatin1String("total_price")] = finalTotal;
    orderData[QLatin1String("discount")] = checkoutDialog.discount();
    orderData[QLatin1String("coupon_code")] = checkoutDialog.couponCode();
    orderData[QLatin1String("payment_method")] = paymentMethod;
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

    // Wallet charge + coupon consumption happen only after the order and
    // its items are safely on disk.
    if (paymentMethod == QLatin1String("wallet")) {
        if (!m_dbHandler->payFromWallet(m_userId, finalTotal, newOrderId)) {
            m_dbHandler->updateOrderStatus(newOrderId, QString(OrderStatus::Cancelled));
            QMessageBox::warning(this, QStringLiteral("پرداخت ناموفق"),
                                 QStringLiteral("پرداخت از کیف پول انجام نشد؛ سفارش لغو شد."));
            return;
        }
    }
    if (checkoutDialog.couponId() > 0)
        m_dbHandler->consumeCoupon(checkoutDialog.couponId());

    // Tell the other live sessions about the order WITHOUT re-inserting it
    // (the old flow sent place_order again, creating duplicate rows).
    if (NetworkManager::instance().isConnected()) {
        QJsonObject note;
        note[QLatin1String("command")] = QLatin1String("notify_order_placed");
        note[QLatin1String("order_id")] = newOrderId;
        note[QLatin1String("restaurant_id")] = orderData.value(QLatin1String("restaurant_id"));
        note[QLatin1String("total_price")] = finalTotal;
        note[QLatin1String("customer")] = m_username;
        NetworkManager::instance().sendJson(note);
    }

    // The old code forgot this - the cart stayed full after paying.
    cart->clearCart();
    m_cartPopup->updateContent();

    statusBar()->showMessage(
        QStringLiteral("سفارش شما با شماره %1 ثبت شد ✅").arg(newOrderId), 6000);
    emit historyChanged();

    // Open the live tracker so the customer immediately sees the pipeline.
    if (m_trackerDialog)
        m_trackerDialog->close();
    m_trackerDialog = new OrderTrackerDialog(newOrderId, QString(OrderStatus::Pending), this);
    m_trackerDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_trackerDialog, &QObject::destroyed, this, [this]() {
        m_trackerDialog = nullptr;
    });
    m_trackerDialog->show();
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

    // Non-blocking live tracking: the pipeline dialog updates in place
    // instead of the old modal message box interrupting every step.
    if (!m_trackerDialog || m_trackerDialog->orderId() != orderId) {
        if (m_trackerDialog)
            m_trackerDialog->close();
        m_trackerDialog = new OrderTrackerDialog(orderId, newStatus, this);
        m_trackerDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_trackerDialog, &QObject::destroyed, this, [this]() {
            m_trackerDialog = nullptr;
        });
        m_trackerDialog->show();
    } else {
        m_trackerDialog->setStatus(newStatus);
        m_trackerDialog->raise();
    }

    statusBar()->showMessage(
        QStringLiteral("🚦 وضعیت سفارش #%1: %2")
            .arg(orderId)
            .arg(OrderStatus::toPersian(newStatus)), 8000);
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
                                       m_dbHandler, m_userId, this);
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
