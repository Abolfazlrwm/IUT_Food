#include "restaurantwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include<QLabel>

RestaurantWindow::RestaurantWindow(Restaurant* restaurant, QWidget* parent)
    : QMainWindow(parent), m_restaurant(restaurant)
{
    setupUI();
    setupConnections();
    updateProfileView();
    updateMenuView();
    updateOrdersView();
}

RestaurantWindow::~RestaurantWindow()
{
}

void RestaurantWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    QTabWidget* tabWidget = new QTabWidget();

    // --------- Profile Tab ----------
    QWidget* profileTab = new QWidget();
    QFormLayout* profileForm = new QFormLayout(profileTab);

    m_nameEdit = new QLineEdit();
    m_typeEdit = new QLineEdit();
    m_locationEdit = new QLineEdit();
    m_priceRangeCombo = new QComboBox();
    m_priceRangeCombo->addItems({"Cheap", "Medium", "Expensive"});
    m_saveProfileButton = new QPushButton("Save Profile");

    profileForm->addRow("Name:", m_nameEdit);
    profileForm->addRow("Cuisine Type:", m_typeEdit);
    profileForm->addRow("Location:", m_locationEdit);
    profileForm->addRow("Price Range:", m_priceRangeCombo);
    profileForm->addRow(m_saveProfileButton);

    tabWidget->addTab(profileTab, "Profile");

    // --------- Menu Tab ----------
    QWidget* menuTab = new QWidget();
    QVBoxLayout* menuLayout = new QVBoxLayout(menuTab);
    QFormLayout* foodForm = new QFormLayout();

    m_foodNameEdit = new QLineEdit();
    m_foodPriceEdit = new QLineEdit();
    m_foodDescEdit = new QTextEdit();
    m_addFoodButton = new QPushButton("Add Food");
    m_editFoodButton = new QPushButton("Edit Food");
    m_deleteFoodButton = new QPushButton("Delete Food");
    m_editFoodButton->setEnabled(false);
    m_deleteFoodButton->setEnabled(false);

    foodForm->addRow("Food Name:", m_foodNameEdit);
    foodForm->addRow("Price:", m_foodPriceEdit);
    foodForm->addRow("Description:", m_foodDescEdit);
    foodForm->addRow(m_addFoodButton);
    foodForm->addRow(m_editFoodButton);
    foodForm->addRow(m_deleteFoodButton);

    m_menuTable = new QTableWidget(0, 4);
    m_menuTable->setHorizontalHeaderLabels({"ID", "Name", "Price", "Description"});
    m_menuTable->setColumnHidden(0, true);
    m_menuTable->horizontalHeader()->setStretchLastSection(true);
    m_menuTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_menuTable->setSelectionMode(QAbstractItemView::SingleSelection);

    menuLayout->addLayout(foodForm);
    menuLayout->addWidget(m_menuTable);

    tabWidget->addTab(menuTab, "Menu");

    // --------- Orders Tab ----------
    QWidget* ordersTab = new QWidget();
    QVBoxLayout* ordersLayout = new QVBoxLayout(ordersTab);

    m_ordersTable = new QTableWidget(0, 5);  // اضافه کردن ستون زمان
    m_ordersTable->setHorizontalHeaderLabels({"ID", "Customer", "Status", "Total Price", "Created At"});
    m_ordersTable->setColumnHidden(0, true);  // مخفی کردن ID
    m_ordersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ordersTable->setSelectionMode(QAbstractItemView::SingleSelection);


    m_confirmOrderButton = new QPushButton("Confirm Order");
    m_shipOrderButton = new QPushButton("Ship Order");
    m_confirmOrderButton->setEnabled(false);
    m_shipOrderButton->setEnabled(false);

    QHBoxLayout* orderButtons = new QHBoxLayout();
    orderButtons->addWidget(m_confirmOrderButton);
    orderButtons->addWidget(m_shipOrderButton);

    ordersLayout->addWidget(m_ordersTable);
    ordersLayout->addLayout(orderButtons);

    tabWidget->addTab(ordersTab, "Orders");

    // Add tab widget to main layout
    mainLayout->addWidget(tabWidget);

    setCentralWidget(centralWidget);
    resize(900, 600);
    // Apply a modern flat style with light theme
    QString styleSheet = R"(
    QWidget {
        font-family: "Segoe UI", sans-serif;
        font-size: 14px;
        background-color: #f4f6f8;
        color: #333;
    }

    QLineEdit, QTextEdit, QComboBox, QTableWidget {
        border: 1px solid #ccc;
        border-radius: 6px;
        padding: 6px;
        background-color: #ffffff;
    }

    QTableWidget {
        gridline-color: #ddd;
        selection-background-color: #cce5ff;
    }

    QPushButton {
        background-color: #0078d7;
        color: white;
        border: none;
        border-radius: 6px;
        padding: 6px 12px;
    }

    QPushButton:hover {
        background-color: #005fa3;
    }

    QPushButton:disabled {
        background-color: #cccccc;
        color: #666666;
    }

    QTabWidget::pane {
        border: 1px solid #aaa;
        border-radius: 8px;
    }

    QTabBar::tab {
        background: #e0e0e0;
        border: 1px solid #ccc;
        border-bottom: none;
        padding: 8px 16px;
        border-top-left-radius: 8px;
        border-top-right-radius: 8px;
        margin-right: 2px;
    }

    QTabBar::tab:selected {
        background: #ffffff;
        font-weight: bold;
    }
)";
    this->setStyleSheet(styleSheet);

}

void RestaurantWindow::setupConnections()
{
    connect(m_saveProfileButton, &QPushButton::clicked, this, &RestaurantWindow::on_saveProfileButton_clicked);
    connect(m_addFoodButton, &QPushButton::clicked, this, &RestaurantWindow::on_addFoodButton_clicked);
    connect(m_editFoodButton, &QPushButton::clicked, this, &RestaurantWindow::on_editFoodButton_clicked);
    connect(m_deleteFoodButton, &QPushButton::clicked, this, &RestaurantWindow::on_deleteFoodButton_clicked);

    // اتصال برای تغییر انتخاب در جدول منو
    connect(m_menuTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool hasSelection = m_menuTable->currentRow() >= 0;
        m_editFoodButton->setEnabled(hasSelection);
        m_deleteFoodButton->setEnabled(hasSelection);

        if (hasSelection) {
            int row = m_menuTable->currentRow();
            int foodId = m_menuTable->item(row, 0)->text().toInt();
            FoodItem item = m_restaurant->getFoodItemById(foodId);
            m_foodNameEdit->setText(item.name);
            m_foodPriceEdit->setText(QString::number(item.price));
            m_foodDescEdit->setText(item.description);
        }
    });

    // اتصال برای تغییر انتخاب در جدول سفارشات
    connect(m_ordersTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        int row = m_ordersTable->currentRow();
        if (row >= 0) {
            QString status = m_ordersTable->item(row, 2)->text();
            m_confirmOrderButton->setEnabled(status == "pending");
            m_shipOrderButton->setEnabled(status == "confirmed");
        } else {
            m_confirmOrderButton->setEnabled(false);
            m_shipOrderButton->setEnabled(false);
        }
    });

    connect(m_confirmOrderButton, &QPushButton::clicked, this, &RestaurantWindow::on_confirmOrderButton_clicked);
    connect(m_shipOrderButton, &QPushButton::clicked, this, &RestaurantWindow::on_shipOrderButton_clicked);

    connect(m_restaurant, &Restaurant::profileChanged, this, &RestaurantWindow::updateProfileView);
    connect(m_restaurant, &Restaurant::menuChanged, this, &RestaurantWindow::updateMenuView);
    connect(m_restaurant, &Restaurant::ordersChanged, this, &RestaurantWindow::updateOrdersView);
    connect(m_restaurant, &Restaurant::newOrderReceived, this, &RestaurantWindow::showNewOrderNotification);
}

void RestaurantWindow::updateProfileView()
{
    m_nameEdit->setText(m_restaurant->getName());
    m_typeEdit->setText(m_restaurant->getType());
    m_locationEdit->setText(m_restaurant->getLocation());
    m_priceRangeCombo->setCurrentIndex(m_restaurant->getPriceRange());
}

void RestaurantWindow::updateMenuView()
{
    m_menuTable->setRowCount(0);
    const auto& menu = m_restaurant->getMenu();
    for(const auto& food : menu) {
        int row = m_menuTable->rowCount();
        m_menuTable->insertRow(row);

        m_menuTable->setItem(row, 0, new QTableWidgetItem(QString::number(food.id)));
        m_menuTable->setItem(row, 1, new QTableWidgetItem(food.name));
        m_menuTable->setItem(row, 2, new QTableWidgetItem(QString::number(food.price)));
        m_menuTable->setItem(row, 3, new QTableWidgetItem(food.description));
    }
}

void RestaurantWindow::updateOrdersView()
{
    m_ordersTable->setRowCount(0);
    const auto& orders = m_restaurant->getOrders();
    for(const auto& order : orders) {
        int row = m_ordersTable->rowCount();
        m_ordersTable->insertRow(row);

        m_ordersTable->setItem(row, 0, new QTableWidgetItem(QString::number(order.id)));
        m_ordersTable->setItem(row, 1, new QTableWidgetItem(order.customerUsername));
        m_ordersTable->setItem(row, 2, new QTableWidgetItem(order.status));
        m_ordersTable->setItem(row, 3, new QTableWidgetItem(QString::number(order.totalPrice)));
    }
}

void RestaurantWindow::on_saveProfileButton_clicked()
{
    if(m_nameEdit->text().isEmpty() || m_typeEdit->text().isEmpty() || m_locationEdit->text().isEmpty()) {
        QMessageBox::warning(this, "خطا", "لطفاً تمام فیلدهای ضروری را پر کنید");
        return;
    }

    m_restaurant->setName(m_nameEdit->text());
    m_restaurant->setType(m_typeEdit->text());
    m_restaurant->setLocation(m_locationEdit->text());
    m_restaurant->setPriceRange(m_priceRangeCombo->currentIndex());

    if (m_restaurant->saveProfileToDB()) {
        QMessageBox::information(this, "موفقیت", "اطلاعات با موفقیت ذخیره شد");
    } else {
        QMessageBox::critical(this, "خطا", "خطا در ذخیره اطلاعات");
    }
}

void RestaurantWindow::on_addFoodButton_clicked()
{
    // اعتبارسنجی ورودی‌ها
    if(m_foodNameEdit->text().isEmpty()) {
        QMessageBox::warning(this, "خطا", "لطفاً نام غذا را وارد کنید");
        return;
    }

    bool priceOk;
    double price = m_foodPriceEdit->text().toDouble(&priceOk);

    if(!priceOk || price <= 0) {
        QMessageBox::warning(this, "خطا", "قیمت وارد شده نامعتبر است");
        return;
    }

    if(m_restaurant->addFood(m_foodNameEdit->text(), price, m_foodDescEdit->toPlainText())) {
        // پاک کردن فیلدها بعد از اضافه شدن
        m_foodNameEdit->clear();
        m_foodPriceEdit->clear();
        m_foodDescEdit->clear();
        QMessageBox::information(this, "موفقیت", "غذا با موفقیت اضافه شد");
    } else {
        QMessageBox::critical(this, "خطا", "خطا در افزودن غذا به دیتابیس");
    }
}

void RestaurantWindow::on_editFoodButton_clicked()
{
    int row = m_menuTable->currentRow();
    if (row < 0) return;

    int foodId = m_menuTable->item(row, 0)->text().toInt();
    QString name = m_foodNameEdit->text();
    double price = m_foodPriceEdit->text().toDouble();
    QString desc = m_foodDescEdit->toPlainText();

    if(m_restaurant->updateFood(foodId, name, price, desc)) {
        QMessageBox::information(this, "موفقیت", "غذا با موفقیت ویرایش شد");
    } else {
        QMessageBox::critical(this, "خطا", "خطا در ویرایش غذا");
    }
}

void RestaurantWindow::on_deleteFoodButton_clicked()
{
    int row = m_menuTable->currentRow();
    if (row < 0) return;

    int foodId = m_menuTable->item(row, 0)->text().toInt();
    if(m_restaurant->removeFood(foodId)) {
        QMessageBox::information(this, "موفقیت", "غذا با موفقیت حذف شد");
    } else {
        QMessageBox::critical(this, "خطا", "خطا در حذف غذا");
    }
}

void RestaurantWindow::on_confirmOrderButton_clicked()
{
    int row = m_ordersTable->currentRow();
    if (row < 0) return;

    int orderId = m_ordersTable->item(row, 0)->text().toInt();
    if(m_restaurant->updateOrderStatus(orderId, "confirmed")) {
        QMessageBox::information(this, "موفقیت", "سفارش با موفقیت تایید شد");
    } else {
        QMessageBox::critical(this, "خطا", "خطا در تایید سفارش");
    }
}

void RestaurantWindow::on_shipOrderButton_clicked()
{
    int row = m_ordersTable->currentRow();
    if (row < 0) return;

    int orderId = m_ordersTable->item(row, 0)->text().toInt();
    if(m_restaurant->updateOrderStatus(orderId, "shipped")) {
        QMessageBox::information(this, "موفقیت", "سفارش با موفقیت ارسال شد");
    } else {
        QMessageBox::critical(this, "خطا", "خطا در ارسال سفارش");
    }
}

void RestaurantWindow::showNewOrderNotification(const QString& customerName)
{
    QMessageBox::information(this, "سفارش جدید", "سفارش جدید از: " + customerName + "\nلطفا بخش سفارش‌ها را بررسی کنید.");
}
