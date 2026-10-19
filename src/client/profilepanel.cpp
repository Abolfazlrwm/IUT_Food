#include "profilepanel.h"
#include "databasehandler.h"
#include "networkmanager.h"
#include "orderstatus.h"

#include <QDebug>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTextEdit>
#include <QVBoxLayout>

ProfilePanel::ProfilePanel(DataBaseHandler *dbHandler, int userId,
                           const QString &username, QWidget *parent)
    : QWidget(parent), m_dbHandler(dbHandler), m_userId(userId), m_username(username)
{
    setLayoutDirection(Qt::RightToLeft);
    setFixedSize(580, 560);
    setupUi();
}

void ProfilePanel::refreshHistory()
{
    if (!m_historyList || !m_dbHandler)
        return;

    m_historyList->clear();

    // Only THIS customer's orders (the old build listed every order in
    // the database for every user).
    QSqlQuery query = m_dbHandler->getOrdersForCustomer(m_userId);
    while (query.next()) {
        const int orderId = query.value(QStringLiteral("id")).toInt();
        const QString status = query.value(QStringLiteral("status")).toString();
        const QString restaurant = query.value(QStringLiteral("restaurant_name")).toString();
        const QString payment = query.value(QStringLiteral("payment_method")).toString();
        const double discount = query.value(QStringLiteral("discount")).toDouble();

        QString displayText = QStringLiteral("سفارش #%1 — %2 — وضعیت: %3")
                                  .arg(orderId)
                                  .arg(restaurant, OrderStatus::toPersian(status));
        if (discount > 0)
            displayText += QStringLiteral(" — 🎟 تخفیف: %1 تومان")
                               .arg(QString::number(discount, 'f', 0));
        if (payment == QLatin1String("wallet"))
            displayText += QStringLiteral(" — 🪙 کیف پول");
        auto *item = new QListWidgetItem(displayText, m_historyList);
        item->setData(Qt::UserRole, orderId);
    }
}

void ProfilePanel::refreshWallet()
{
    if (!m_dbHandler)
        return;
    if (m_walletBalanceLabel)
        m_walletBalanceLabel->setText(
            QStringLiteral("%1 تومان")
                .arg(QString::number(m_dbHandler->getWalletBalance(m_userId), 'f', 0)));
    if (m_walletTxList) {
        m_walletTxList->clear();
        QSqlQuery tx = m_dbHandler->getWalletTransactions(m_userId);
        while (tx.next()) {
            const double amount = tx.value(QStringLiteral("amount")).toDouble();
            const QString kind = tx.value(QStringLiteral("kind")).toString();
            const int orderId = tx.value(QStringLiteral("order_id")).toInt();
            QString text;
            if (kind == QLatin1String("topup"))
                text = QStringLiteral("🟢 شارژ کیف پول — %1 تومان")
                           .arg(QString::number(amount, 'f', 0));
            else
                text = QStringLiteral("🔵 پرداخت سفارش #%1 — %2 تومان")
                           .arg(orderId)
                           .arg(QString::number(amount, 'f', 0));
            text += QStringLiteral("  (%1)").arg(tx.value(QStringLiteral("created_at")).toString());
            m_walletTxList->addItem(text);
        }
    }
}

void ProfilePanel::setupUi()
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    m_navigationList = new QListWidget(this);
    m_navigationList->setFixedWidth(170);
    m_navigationList->addItem(QStringLiteral("📅 تاریخچه سفارشات"));
    m_navigationList->addItem(QStringLiteral("💳 کیف پول"));
    m_navigationList->addItem(QStringLiteral("⚙️ تنظیمات پروفایل"));

    m_contentStack = new QStackedWidget(this);
    m_contentStack->addWidget(createHistoryPage());   // index 0
    m_contentStack->addWidget(createWalletPage());    // index 1
    m_contentStack->addWidget(createSettingsPage());  // index 2
    m_contentStack->addWidget(createOrderDetailPage());// index 3

    mainLayout->addWidget(m_contentStack, 1);
    mainLayout->addWidget(m_navigationList);

    connect(m_navigationList, &QListWidget::currentRowChanged, this, [this](int index) {
        if (index < 0)
            return;
        m_navigationList->setVisible(true);
        if (index == 0)
            refreshHistory();
        else if (index == 1)
            refreshWallet();
        m_contentStack->setCurrentIndex(index);
    });

    m_navigationList->setCurrentRow(0);
    refreshHistory();
}

QWidget *ProfilePanel::createHistoryPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    m_historyList = new QListWidget(page);
    layout->addWidget(new QLabel(QStringLiteral("تاریخچه سفارشات شما:"), page));
    layout->addWidget(m_historyList);

    auto *reorderButton = new QPushButton(QStringLiteral("🔄 سفارش دوباره"), page);
    reorderButton->setEnabled(false);
    layout->addWidget(reorderButton);

    connect(m_historyList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        loadOrderDetails(item->data(Qt::UserRole).toInt());
    });
    connect(m_historyList, &QListWidget::itemSelectionChanged, this, [this, reorderButton]() {
        reorderButton->setEnabled(m_historyList->currentItem() != nullptr);
    });
    connect(reorderButton, &QPushButton::clicked, this, &ProfilePanel::reorderSelectedOrder);
    return page;
}

//! Push every dish of the selected order back into the shopping cart.
void ProfilePanel::reorderSelectedOrder()
{
    auto *item = m_historyList->currentItem();
    if (!item || !m_dbHandler)
        return;
    const int orderId = item->data(Qt::UserRole).toInt();

    int restaurantId = -1;
    QSqlQuery orderQuery = m_dbHandler->getOrderDetails(orderId);
    if (orderQuery.next())
        restaurantId = orderQuery.value(QStringLiteral("restaurant_id")).toInt();
    if (restaurantId <= 0)
        return;

    QSqlQuery itemsQuery = m_dbHandler->getOrderItems(orderId);
    int added = 0;
    while (itemsQuery.next()) {
        QJsonObject foodData;
        foodData[QLatin1String("id")] = itemsQuery.value(QStringLiteral("food_id")).toInt();
        foodData[QLatin1String("name")] = itemsQuery.value(QStringLiteral("food_name")).toString();
        foodData[QLatin1String("price")] = itemsQuery.value(QStringLiteral("price_per_item")).toDouble();
        foodData[QLatin1String("restaurant_id")] = restaurantId;
        emit reorderRequested(foodData, itemsQuery.value(QStringLiteral("quantity")).toInt());
        ++added;
    }

    if (added > 0)
        QMessageBox::information(this, QStringLiteral("سفارش دوباره"),
                                 QStringLiteral("%1 آیتم به سبد خرید اضافه شد 🛒\n"
                                                "از دکمه سبد خرید می‌توانید checkout کنید.").arg(added));
}

QWidget *ProfilePanel::createWalletPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(10);

    auto *balanceBox = new QGroupBox(QStringLiteral("موجودی کیف پول"), page);
    auto *balanceLayout = new QVBoxLayout(balanceBox);
    m_walletBalanceLabel = new QLabel(QStringLiteral("0 تومان"), balanceBox);
    m_walletBalanceLabel->setObjectName(QStringLiteral("walletBalanceLabel"));
    m_walletBalanceLabel->setAlignment(Qt::AlignCenter);
    m_walletBalanceLabel->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold;"));
    balanceLayout->addWidget(m_walletBalanceLabel);

    auto *chargeButton = new QPushButton(QStringLiteral("➕ شارژ کیف پول"), balanceBox);
    balanceLayout->addWidget(chargeButton);
    layout->addWidget(balanceBox);

    layout->addWidget(new QLabel(QStringLiteral("آخرین تراکنش‌ها:"), page));
    m_walletTxList = new QListWidget(page);
    layout->addWidget(m_walletTxList, 1);

    connect(chargeButton, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const double amount = QInputDialog::getDouble(
            this, QStringLiteral("شارژ کیف پول"),
            QStringLiteral("مبلغ (تومان):"), 100000, 10000, 50000000, 1000, &ok);
        if (!ok)
            return;
        if (!m_dbHandler->chargeWallet(m_userId, amount, QStringLiteral("شارژ دستی"))) {
            QMessageBox::warning(this, QStringLiteral("خطا"),
                                 QStringLiteral("شارژ کیف پول ناموفق بود."));
            return;
        }
        refreshWallet();
        QMessageBox::information(this, QStringLiteral("شارژ شد"),
                                 QStringLiteral("کیف پول شما با موفقیت شارژ شد ✅"));
    });

    return page;
}

QWidget *ProfilePanel::createSettingsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *form = new QFormLayout;
    form->setContentsMargins(20, 20, 20, 20);
    m_nameEdit = new QLineEdit;
    m_addressEdit = new QLineEdit;
    m_addressEdit->setPlaceholderText(QStringLiteral("مثلاً: دانشکده کامپیوتر، طبقه ۲"));
    form->addRow(QStringLiteral("نام نمایشی:"), m_nameEdit);
    form->addRow(QStringLiteral("آدرس:"), m_addressEdit);
    layout->addLayout(form);

    auto *saveButton = new QPushButton(QStringLiteral("ذخیره تغییرات"), page);
    layout->addWidget(saveButton);
    layout->addStretch();

    // The old settings page created its fields inside the function body
    // and never connected the button - nothing was ever saved.
    QSqlQuery details = m_dbHandler->getUserDetails(m_userId);
    if (details.next()) {
        m_nameEdit->setText(details.value(QStringLiteral("username")).toString());
        m_addressEdit->setText(details.value(QStringLiteral("address")).toString());
    }

    connect(saveButton, &QPushButton::clicked, this, [this]() {
        const QString newName = m_nameEdit->text().trimmed();
        if (newName.isEmpty() || !m_dbHandler->updateUserDetails(m_userId, newName,
                                                                 m_addressEdit->text().trimmed())) {
            QMessageBox::warning(this, QStringLiteral("خطا"),
                                 QStringLiteral("ذخیره اطلاعات ناموفق بود (نام ممکن است تکراری باشد)."));
            return;
        }
        m_username = newName;
        QMessageBox::information(this, QStringLiteral("ذخیره شد"),
                                 QStringLiteral("اطلاعات پروفایل به‌روزرسانی شد."));
        refreshHistory();
    });

    return page;
}

QWidget *ProfilePanel::createOrderDetailPage()
{
    auto *page = new QWidget();
    auto *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(8);

    auto *backButton = new QPushButton(QStringLiteral("بازگشت به لیست"), page);
    connect(backButton, &QPushButton::clicked, this, [this]() {
        refreshHistory();
        m_contentStack->setCurrentIndex(0);
        m_navigationList->setVisible(true);
    });

    auto *orderIdLabel = new QLabel(QStringLiteral("سفارش: -"), page);
    orderIdLabel->setObjectName(QStringLiteral("orderIdLabel"));
    orderIdLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 15px;"));

    auto *statusLabel = new QLabel(QStringLiteral("وضعیت: -"), page);
    statusLabel->setObjectName(QStringLiteral("statusLabel"));

    auto *paymentLabel = new QLabel(QStringLiteral("پرداخت: -"), page);
    paymentLabel->setObjectName(QStringLiteral("paymentLabel"));

    auto *itemsList = new QListWidget(page);
    itemsList->setObjectName(QStringLiteral("itemsList"));
    itemsList->setMaximumHeight(110);

    auto *deliveredButton = new QPushButton(QStringLiteral("✓ دریافت کردم"), page);
    deliveredButton->setObjectName(QStringLiteral("deliveredButton"));
    deliveredButton->setVisible(false);
    connect(deliveredButton, &QPushButton::clicked, this, &ProfilePanel::markOrderDelivered);

    auto *chatHistory = new QTextEdit(page);
    chatHistory->setReadOnly(true);
    chatHistory->setObjectName(QStringLiteral("chatHistory"));

    auto *messageInput = new QLineEdit(page);
    messageInput->setPlaceholderText(QStringLiteral("پیام خود را بنویسید…"));
    messageInput->setObjectName(QStringLiteral("messageInput"));

    auto *sendButton = new QPushButton(QStringLiteral("ارسال"), page);
    connect(sendButton, &QPushButton::clicked, this, [this, messageInput, chatHistory]() {
        if (messageInput->text().isEmpty() || m_currentOrderId == -1)
            return;
        QJsonObject chatRequest;
        chatRequest[QLatin1String("command")] = QLatin1String("send_chat_message");
        chatRequest[QLatin1String("order_id")] = m_currentOrderId;
        chatRequest[QLatin1String("sender")] = m_username;
        chatRequest[QLatin1String("message_text")] = messageInput->text();
        NetworkManager::instance().sendJson(chatRequest);
        chatHistory->append(QStringLiteral("شما: %1").arg(messageInput->text()));
        messageInput->clear();
    });

    auto *ratingBox = new QGroupBox(QStringLiteral("ثبت امتیاز و نظر"), page);
    ratingBox->setObjectName(QStringLiteral("ratingBox"));
    ratingBox->setVisible(false);
    auto *ratingLayout = new QFormLayout(ratingBox);
    auto *ratingSpinBox = new QSpinBox(ratingBox);
    ratingSpinBox->setRange(1, 5);
    ratingSpinBox->setValue(5);
    ratingSpinBox->setSuffix(QStringLiteral(" ستاره"));
    auto *reviewTextEdit = new QTextEdit(ratingBox);
    reviewTextEdit->setPlaceholderText(QStringLiteral("نظر خود را بنویسید…"));
    reviewTextEdit->setMaximumHeight(70);
    auto *submitReviewButton = new QPushButton(QStringLiteral("ثبت نهایی نظر"), ratingBox);
    connect(submitReviewButton, &QPushButton::clicked, this, [this, ratingSpinBox, reviewTextEdit, ratingBox]() {
        m_currentRating = ratingSpinBox->value();
        // Save locally (always available) and mirror to the server when online.
        if (m_dbHandler->submitReview(m_currentOrderId, m_currentRestaurantId, m_userId,
                                      m_currentRating, reviewTextEdit->toPlainText().trimmed())) {
            NetworkManager::instance().sendJson(QJsonObject{
                {QStringLiteral("command"), QStringLiteral("submit_review")},
                {QStringLiteral("order_id"), m_currentOrderId},
                {QStringLiteral("rating"), m_currentRating},
                {QStringLiteral("review_text"), reviewTextEdit->toPlainText().trimmed()}});
            ratingBox->setTitle(QStringLiteral("نظر شما ثبت شد؛ ممنون!"));
            ratingBox->setEnabled(false);
        } else {
            QMessageBox::warning(this, QStringLiteral("خطا"),
                                 QStringLiteral("ثبت نظر ناموفق بود."));
        }
    });
    ratingLayout->addRow(QStringLiteral("امتیاز شما:"), ratingSpinBox);
    ratingLayout->addRow(QStringLiteral("متن نظر:"), reviewTextEdit);
    ratingLayout->addRow(submitReviewButton);

    mainLayout->addWidget(backButton);
    mainLayout->addWidget(orderIdLabel);
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(paymentLabel);
    mainLayout->addWidget(new QLabel(QStringLiteral("اقلام سفارش:"), page));
    mainLayout->addWidget(itemsList);
    mainLayout->addWidget(deliveredButton);
    mainLayout->addWidget(new QLabel(QStringLiteral("گفت‌وگو با رستوران:"), page));
    mainLayout->addWidget(chatHistory, 1);
    mainLayout->addWidget(messageInput);
    mainLayout->addWidget(sendButton);
    mainLayout->addWidget(ratingBox);

    return page;
}

void ProfilePanel::loadOrderDetails(int orderId)
{
    if (orderId <= 0)
        return;
    m_currentOrderId = orderId;
    m_currentRating = 0;

    QString status;
    QString paymentMethod;
    double discount = 0.0;
    QString couponCode;
    bool alreadyReviewed = false;
    QSqlQuery orderQuery = m_dbHandler->getOrderDetails(orderId);
    if (orderQuery.next()) {
        status = orderQuery.value(QStringLiteral("status")).toString();
        m_currentRestaurantId = orderQuery.value(QStringLiteral("restaurant_id")).toInt();
        alreadyReviewed = orderQuery.value(QStringLiteral("review_submitted")).toInt() == 1;
        paymentMethod = orderQuery.value(QStringLiteral("payment_method")).toString();
        discount = orderQuery.value(QStringLiteral("discount")).toDouble();
        couponCode = orderQuery.value(QStringLiteral("coupon_code")).toString();
    } else {
        m_currentRestaurantId = -1;
        return;
    }

    QWidget *detailPage = m_contentStack->widget(3);
    if (!detailPage || !m_dbHandler)
        return;

    auto *idLabel = detailPage->findChild<QLabel *>(QStringLiteral("orderIdLabel"));
    auto *statusLabel = detailPage->findChild<QLabel *>(QStringLiteral("statusLabel"));
    auto *paymentLabel = detailPage->findChild<QLabel *>(QStringLiteral("paymentLabel"));
    auto *itemsList = detailPage->findChild<QListWidget *>(QStringLiteral("itemsList"));
    auto *deliveredButton = detailPage->findChild<QPushButton *>(QStringLiteral("deliveredButton"));

    idLabel->setText(QStringLiteral("سفارش شماره: %1").arg(orderId));
    statusLabel->setText(QStringLiteral("وضعیت: %1").arg(OrderStatus::toPersian(status)));

    QString paymentText = (paymentMethod == QLatin1String("wallet"))
                              ? QStringLiteral("پرداخت: کیف پول 🪙")
                              : QStringLiteral("پرداخت: نقدی 💵");
    if (discount > 0)
        paymentText += QStringLiteral(" — 🎟 تخفیف (%1): %2 تومان")
                           .arg(couponCode)
                           .arg(QString::number(discount, 'f', 0));
    paymentLabel->setText(paymentText);

    // Single pass over the items (the old build had a nested duplicate
    // loop that appended every item twice).
    itemsList->clear();
    QSqlQuery itemsQuery = m_dbHandler->getOrderItems(orderId);
    while (itemsQuery.next()) {
        const QString name = itemsQuery.value(QStringLiteral("food_name")).toString();
        const int quantity = itemsQuery.value(QStringLiteral("quantity")).toInt();
        const double price = itemsQuery.value(QStringLiteral("price_per_item")).toDouble();
        itemsList->addItem(QStringLiteral("%1 ×%2 — %3 تومان")
                               .arg(name).arg(quantity).arg(price * quantity));
    }

    const bool canConfirmDelivery = (status == OrderStatus::Shipped);
    deliveredButton->setVisible(canConfirmDelivery);

    auto *ratingBox = detailPage->findChild<QGroupBox *>(QStringLiteral("ratingBox"));
    if (ratingBox) {
        const bool isDelivered = (status == OrderStatus::Delivered);
        ratingBox->setVisible(isDelivered);
        ratingBox->setEnabled(isDelivered && !alreadyReviewed);
        if (isDelivered && alreadyReviewed)
            ratingBox->setTitle(QStringLiteral("نظر شما قبلاً ثبت شده"));
    }

    m_contentStack->setCurrentIndex(3);
    m_navigationList->setVisible(false);
}

void ProfilePanel::markOrderDelivered()
{
    if (m_currentOrderId <= 0)
        return;

    if (!m_dbHandler->updateOrderStatus(m_currentOrderId, QString(OrderStatus::Delivered))) {
        QMessageBox::warning(this, QStringLiteral("خطا"), QStringLiteral("به‌روزرسانی وضعیت ناموفق بود."));
        return;
    }

    // Mirror the transition to the server so the restaurant dashboard and
    // any other session stay in sync.
    NetworkManager::instance().sendJson(QJsonObject{
        {QStringLiteral("command"), QStringLiteral("update_order_status")},
        {QStringLiteral("order_id"), m_currentOrderId},
        {QStringLiteral("new_status"), QString(OrderStatus::Delivered)}});

    loadOrderDetails(m_currentOrderId); // refresh the page (shows the review box now)
}
