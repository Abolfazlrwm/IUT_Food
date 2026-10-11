#include "restaurantwindow.h"
#include "networkmanager.h"
#include "orderstatus.h"
#include "restaurant.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

RestaurantWindow::RestaurantWindow(Restaurant *restaurant, QWidget *parent)
    : QMainWindow(parent), m_restaurant(restaurant)
{
    setupUi();
    setupConnections();
    updateProfileView();
    updateMenuView();
    updateOrdersView();

    // Live chat + push notifications over the shared TCP connection.
    NetworkManager &net = NetworkManager::instance();
    net.connectToServer();
    connect(&net, &NetworkManager::chatMessageReceived, this,
            [this](const QJsonObject &data) {
                const QString sender = data.value(QLatin1String("sender")).toString();
                const QString text = data.value(QLatin1String("message_text")).toString();
                const int orderId = data.value(QLatin1String("order_id")).toInt();
                m_chatHistory->append(
                    QStringLiteral("مشتری (%1 — سفارش #%2): %3").arg(sender).arg(orderId).arg(text));
            });
    connect(&net, &NetworkManager::orderStatusUpdated, this, &RestaurantWindow::updateOrdersView);
}

RestaurantWindow::~RestaurantWindow() = default;

void RestaurantWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *tabWidget = new QTabWidget();

    // ----------------------------- profile tab -------------------------
    auto *profileTab = new QWidget();
    auto *profileForm = new QFormLayout(profileTab);

    m_nameEdit = new QLineEdit();
    m_typeEdit = new QLineEdit();
    m_locationEdit = new QLineEdit();
    m_priceRangeCombo = new QComboBox();
    m_priceRangeCombo->addItems({QStringLiteral("اقتصادی"), QStringLiteral("متوسط"),
                                 QStringLiteral("گران")});
    m_saveProfileButton = new QPushButton(QStringLiteral("ذخیره اطلاعات"));

    profileForm->addRow(QStringLiteral("نام رستوران:"), m_nameEdit);
    profileForm->addRow(QStringLiteral("نوع غذا:"), m_typeEdit);
    profileForm->addRow(QStringLiteral("منطقه:"), m_locationEdit);
    profileForm->addRow(QStringLiteral("محدوده قیمت:"), m_priceRangeCombo);
    profileForm->addRow(m_saveProfileButton);

    tabWidget->addTab(profileTab, QStringLiteral("پروفایل"));

    // ----------------------------- menu tab ----------------------------
    auto *menuTab = new QWidget();
    auto *menuLayout = new QHBoxLayout(menuTab);

    auto *menuFormHolder = new QWidget();
    auto *foodForm = new QFormLayout(menuFormHolder);
    foodForm->setContentsMargins(0, 0, 8, 0);

    m_foodNameEdit = new QLineEdit();
    m_foodPriceEdit = new QLineEdit();
    m_foodDescEdit = new QTextEdit();
    m_foodDescEdit->setMaximumHeight(80);
    m_addFoodButton = new QPushButton(QStringLiteral("افزودن غذا"));
    m_editFoodButton = new QPushButton(QStringLiteral("ویرایش غذا"));
    m_deleteFoodButton = new QPushButton(QStringLiteral("حذف غذا"));
    m_editFoodButton->setEnabled(false);
    m_deleteFoodButton->setEnabled(false);

    foodForm->addRow(QStringLiteral("نام غذا:"), m_foodNameEdit);
    foodForm->addRow(QStringLiteral("قیمت (تومان):"), m_foodPriceEdit);
    foodForm->addRow(QStringLiteral("توضیحات:"), m_foodDescEdit);
    foodForm->addRow(m_addFoodButton);
    foodForm->addRow(m_editFoodButton);
    foodForm->addRow(m_deleteFoodButton);

    m_menuTable = new QTableWidget(0, 4);
    m_menuTable->setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("نام"),
                                            QStringLiteral("قیمت"), QStringLiteral("توضیحات")});
    m_menuTable->setColumnHidden(0, true);
    m_menuTable->horizontalHeader()->setStretchLastSection(true);
    m_menuTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_menuTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_menuTable->setMinimumWidth(380);

    menuLayout->addWidget(menuFormHolder);
    menuLayout->addWidget(m_menuTable, 1);

    tabWidget->addTab(menuTab, QStringLiteral("منو"));

    // ----------------------------- orders tab ---------------------------
    auto *ordersTab = new QWidget();
    auto *ordersLayout = new QVBoxLayout(ordersTab);

    m_ordersTable = new QTableWidget(0, 5);
    m_ordersTable->setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("مشتری"),
                                              QStringLiteral("وضعیت"), QStringLiteral("مبلغ (تومان)"),
                                              QStringLiteral("زمان ثبت")});
    m_ordersTable->setColumnHidden(0, true);
    m_ordersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ordersTable->setSelectionMode(QAbstractItemView::SingleSelection);

    m_confirmOrderButton = new QPushButton(QStringLiteral("تأیید سفارش"));
    m_shipOrderButton = new QPushButton(QStringLiteral("ارسال پیک"));
    m_confirmOrderButton->setEnabled(false);
    m_shipOrderButton->setEnabled(false);

    auto *orderButtons = new QHBoxLayout();
    orderButtons->addWidget(m_confirmOrderButton);
    orderButtons->addWidget(m_shipOrderButton);

    ordersLayout->addWidget(m_ordersTable);
    ordersLayout->addLayout(orderButtons);

    tabWidget->addTab(ordersTab, QStringLiteral("سفارش‌ها"));

    // ----------------------------- chat tab -----------------------------
    auto *chatTab = new QWidget();
    auto *chatLayout = new QVBoxLayout(chatTab);
    m_chatHistory = new QTextEdit();
    m_chatHistory->setReadOnly(true);
    m_chatHistory->setPlaceholderText(QStringLiteral("پیام‌های مشتریان اینجا نمایش داده می‌شود…"));
    m_chatInput = new QLineEdit();
    m_chatInput->setPlaceholderText(QStringLiteral("پاسخ خود را بنویسید…"));
    m_chatSendButton = new QPushButton(QStringLiteral("ارسال پاسخ"));
    auto *chatInputRow = new QHBoxLayout();
    chatInputRow->addWidget(m_chatInput, 1);
    chatInputRow->addWidget(m_chatSendButton);
    chatLayout->addWidget(new QLabel(QStringLiteral("💬  گفت‌وگوی زنده با مشتریان (از طریق سرور)")));
    chatLayout->addWidget(m_chatHistory, 1);
    chatLayout->addLayout(chatInputRow);

    tabWidget->addTab(chatTab, QStringLiteral("گفت‌وگو"));

    mainLayout->addWidget(tabWidget);
    setCentralWidget(centralWidget);
    resize(940, 620);
    setLayoutDirection(Qt::RightToLeft);
    setWindowTitle(QStringLiteral("IUT-Food — پنل رستوران"));

    this->setStyleSheet(QStringLiteral(R"(
    QWidget { font-family: "Vazirmatn", "Segoe UI", sans-serif; font-size: 13px; background-color: #f4f6f8; color: #333; }
    QLineEdit, QTextEdit, QComboBox, QTableWidget { border: 1px solid #ccc; border-radius: 6px; padding: 6px; background-color: #ffffff; }
    QTableWidget { gridline-color: #ddd; selection-background-color: #cce5ff; }
    QPushButton { background-color: #0078d7; color: white; border: none; border-radius: 6px; padding: 8px 14px; }
    QPushButton:hover { background-color: #005fa3; }
    QPushButton:disabled { background-color: #cccccc; color: #666666; }
    QTabWidget::pane { border: 1px solid #aaa; border-radius: 8px; }
    QTabBar::tab { background: #e0e0e0; border: 1px solid #ccc; border-bottom: none; padding: 8px 18px; border-top-left-radius: 8px; border-top-right-radius: 8px; margin-right: 2px; }
    QTabBar::tab:selected { background: #ffffff; font-weight: bold; }
)"));
}

void RestaurantWindow::setupConnections()
{
    connect(m_saveProfileButton, &QPushButton::clicked, this, &RestaurantWindow::on_saveProfileButton_clicked);
    connect(m_addFoodButton, &QPushButton::clicked, this, &RestaurantWindow::on_addFoodButton_clicked);
    connect(m_editFoodButton, &QPushButton::clicked, this, &RestaurantWindow::on_editFoodButton_clicked);
    connect(m_deleteFoodButton, &QPushButton::clicked, this, &RestaurantWindow::on_deleteFoodButton_clicked);
    connect(m_confirmOrderButton, &QPushButton::clicked, this, &RestaurantWindow::on_confirmOrderButton_clicked);
    connect(m_shipOrderButton, &QPushButton::clicked, this, &RestaurantWindow::on_shipOrderButton_clicked);

    connect(m_menuTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int row = m_menuTable->currentRow();
        const bool hasSelection = row >= 0;
        m_editFoodButton->setEnabled(hasSelection);
        m_deleteFoodButton->setEnabled(hasSelection);
        if (!hasSelection)
            return;
        const int foodId = m_menuTable->item(row, 0)->text().toInt();
        const FoodItem item = m_restaurant->getFoodItemById(foodId);
        m_foodNameEdit->setText(item.name);
        m_foodPriceEdit->setText(QString::number(item.price));
        m_foodDescEdit->setPlainText(item.description);
    });

    connect(m_ordersTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int row = m_ordersTable->currentRow();
        if (row < 0) {
            m_confirmOrderButton->setEnabled(false);
            m_shipOrderButton->setEnabled(false);
            return;
        }
        // Canonical status travels in UserRole; the visible column is Persian.
        const QString status = m_ordersTable->item(row, 2)->data(Qt::UserRole).toString();
        m_confirmOrderButton->setEnabled(status == OrderStatus::Pending);
        m_shipOrderButton->setEnabled(status == OrderStatus::Confirmed);
    });

    connect(m_chatSendButton, &QPushButton::clicked, this, [this]() {
        const QString text = m_chatInput->text().trimmed();
        if (text.isEmpty())
            return;
        NetworkManager::instance().sendJson(QJsonObject{
            {QStringLiteral("command"), QStringLiteral("send_chat_message")},
            {QStringLiteral("sender"), m_restaurant->getName()},
            {QStringLiteral("message_text"), text}});
        m_chatHistory->append(QStringLiteral("شما: %1").arg(text));
        m_chatInput->clear();
    });

    connect(m_restaurant, &Restaurant::profileChanged, this, &RestaurantWindow::updateProfileView);
    connect(m_restaurant, &Restaurant::menuChanged, this, &RestaurantWindow::updateMenuView);
    connect(m_restaurant, &Restaurant::ordersChanged, this, &RestaurantWindow::updateOrdersView);
    connect(m_restaurant, &Restaurant::newOrderReceived, this, [this](const QString &customerName) {
        QMessageBox::information(this, QStringLiteral("سفارش جدید"),
                                 QStringLiteral("سفارش جدید از مشتری «%1» ثبت شد.\nبخش سفارش‌ها را بررسی کنید.")
                                     .arg(customerName));
    });
}

void RestaurantWindow::updateProfileView()
{
    m_nameEdit->setText(m_restaurant->getName());
    m_typeEdit->setText(m_restaurant->getType());
    m_locationEdit->setText(m_restaurant->getLocation());
    m_priceRangeCombo->setCurrentIndex(qBound(0, m_restaurant->getPriceRange() - 1, 2));
}

void RestaurantWindow::updateMenuView()
{
    m_menuTable->setRowCount(0);
    const auto &menu = m_restaurant->getMenu();
    for (const auto &food : menu) {
        const int row = m_menuTable->rowCount();
        m_menuTable->insertRow(row);
        m_menuTable->setItem(row, 0, new QTableWidgetItem(QString::number(food.id)));
        m_menuTable->setItem(row, 1, new QTableWidgetItem(food.name));
        m_menuTable->setItem(row, 2, new QTableWidgetItem(QString::number(food.price)));
        m_menuTable->setItem(row, 3, new QTableWidgetItem(food.description));
    }
}

void RestaurantWindow::updateOrdersView()
{
    const int selectedRow = m_ordersTable->currentRow();
    const int selectedOrderId = selectedRow >= 0
        ? m_ordersTable->item(selectedRow, 0)->text().toInt() : -1;

    m_ordersTable->setRowCount(0);
    const auto &orders = m_restaurant->getOrders();
    for (const auto &order : orders) {
        const int row = m_ordersTable->rowCount();
        m_ordersTable->insertRow(row);
        m_ordersTable->setItem(row, 0, new QTableWidgetItem(QString::number(order.id)));
        m_ordersTable->setItem(row, 1, new QTableWidgetItem(order.customerUsername));

        // Visible text is Persian, canonical value rides in UserRole.
        auto *statusItem = new QTableWidgetItem(OrderStatus::toPersian(order.status));
        statusItem->setData(Qt::UserRole, order.status);
        m_ordersTable->setItem(row, 2, statusItem);

        m_ordersTable->setItem(row, 3, new QTableWidgetItem(QString::number(order.totalPrice)));
        // The "Created At" column stayed empty in the original build.
        m_ordersTable->setItem(row, 4, new QTableWidgetItem(order.createdAt.toString(QStringLiteral("yyyy-MM-dd hh:mm"))));

        if (order.id == selectedOrderId)
            m_ordersTable->selectRow(row);
    }
}

void RestaurantWindow::on_saveProfileButton_clicked()
{
    if (m_nameEdit->text().isEmpty() || m_typeEdit->text().isEmpty() || m_locationEdit->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("خطا"), QStringLiteral("لطفاً تمام فیلدهای ضروری را پر کنید"));
        return;
    }
    m_restaurant->setName(m_nameEdit->text());
    m_restaurant->setType(m_typeEdit->text());
    m_restaurant->setLocation(m_locationEdit->text());
    m_restaurant->setPriceRange(m_priceRangeCombo->currentIndex() + 1);

    if (m_restaurant->saveProfileToDB())
        QMessageBox::information(this, QStringLiteral("موفقیت"), QStringLiteral("اطلاعات با موفقیت ذخیره شد"));
    else
        QMessageBox::critical(this, QStringLiteral("خطا"), QStringLiteral("خطا در ذخیره اطلاعات"));
}

void RestaurantWindow::on_addFoodButton_clicked()
{
    if (m_foodNameEdit->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("خطا"), QStringLiteral("لطفاً نام غذا را وارد کنید"));
        return;
    }
    bool priceOk = false;
    const double price = m_foodPriceEdit->text().toDouble(&priceOk);
    if (!priceOk || price <= 0) {
        QMessageBox::warning(this, QStringLiteral("خطا"), QStringLiteral("قیمت وارد شده نامعتبر است"));
        return;
    }

    if (m_restaurant->addFood(m_foodNameEdit->text(), price, m_foodDescEdit->toPlainText())) {
        m_foodNameEdit->clear();
        m_foodPriceEdit->clear();
        m_foodDescEdit->clear();
        QMessageBox::information(this, QStringLiteral("موفقیت"), QStringLiteral("غذا با موفقیت اضافه شد"));
    } else {
        QMessageBox::critical(this, QStringLiteral("خطا"), QStringLiteral("خطا در افزودن غذا به دیتابیس"));
    }
}

void RestaurantWindow::on_editFoodButton_clicked()
{
    const int row = m_menuTable->currentRow();
    if (row < 0)
        return;
    const int foodId = m_menuTable->item(row, 0)->text().toInt();
    bool priceOk = false;
    const double price = m_foodPriceEdit->text().toDouble(&priceOk);
    if (m_foodNameEdit->text().isEmpty() || !priceOk || price <= 0) {
        QMessageBox::warning(this, QStringLiteral("خطا"), QStringLiteral("اطلاعات غذا را کامل و معتبر وارد کنید"));
        return;
    }
    if (m_restaurant->updateFood(foodId, m_foodNameEdit->text(), price, m_foodDescEdit->toPlainText()))
        QMessageBox::information(this, QStringLiteral("موفقیت"), QStringLiteral("غذا با موفقیت ویرایش شد"));
    else
        QMessageBox::critical(this, QStringLiteral("خطا"), QStringLiteral("خطا در ویرایش غذا"));
}

void RestaurantWindow::on_deleteFoodButton_clicked()
{
    const int row = m_menuTable->currentRow();
    if (row < 0)
        return;
    const int foodId = m_menuTable->item(row, 0)->text().toInt();
    if (m_restaurant->removeFood(foodId))
        QMessageBox::information(this, QStringLiteral("موفقیت"), QStringLiteral("غذا با موفقیت حذف شد"));
    else
        QMessageBox::critical(this, QStringLiteral("خطا"), QStringLiteral("خطا در حذف غذا"));
}

void RestaurantWindow::on_confirmOrderButton_clicked()
{
    const int row = m_ordersTable->currentRow();
    if (row < 0)
        return;
    const int orderId = m_ordersTable->item(row, 0)->text().toInt();

    if (m_restaurant->updateOrderStatus(orderId, QString(OrderStatus::Confirmed))) {
        // Mirror to the server so customer sessions get the push update.
        NetworkManager::instance().sendJson(QJsonObject{
            {QStringLiteral("command"), QStringLiteral("update_order_status")},
            {QStringLiteral("order_id"), orderId},
            {QStringLiteral("new_status"), QString(OrderStatus::Confirmed)}});
        QMessageBox::information(this, QStringLiteral("موفقیت"), QStringLiteral("سفارش تأیید شد"));
    } else {
        QMessageBox::critical(this, QStringLiteral("خطا"), QStringLiteral("خطا در تأیید سفارش"));
    }
}

void RestaurantWindow::on_shipOrderButton_clicked()
{
    const int row = m_ordersTable->currentRow();
    if (row < 0)
        return;
    const int orderId = m_ordersTable->item(row, 0)->text().toInt();

    if (m_restaurant->updateOrderStatus(orderId, QString(OrderStatus::Shipped))) {
        NetworkManager::instance().sendJson(QJsonObject{
            {QStringLiteral("command"), QStringLiteral("update_order_status")},
            {QStringLiteral("order_id"), orderId},
            {QStringLiteral("new_status"), QString(OrderStatus::Shipped)}});
        QMessageBox::information(this, QStringLiteral("موفقیت"), QStringLiteral("سفارش به پیک سپرده شد"));
    } else {
        QMessageBox::critical(this, QStringLiteral("خطا"), QStringLiteral("خطا در ارسال سفارش"));
    }
}
