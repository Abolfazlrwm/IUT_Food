#include "adminpanel.h"

AdminPanel::AdminPanel(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    setupConnections();
    loadUsers();
    loadOrders();

    // Connection to server
            m_network = new AdminNetworkManager(this);
    connect(m_network, &AdminNetworkManager::serverResponse,
            this, &AdminPanel::onServerResponse);

    m_network->connectToServer("127.0.0.1", 5555);
}

AdminPanel::~AdminPanel() {}

void AdminPanel::setupUI()
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    tabWidget = new QTabWidget(central);
    mainLayout->addWidget(tabWidget);

    setupUserTab();
    setupOrdersTab();
    setupReportsTab();

    tabWidget->addTab(userTab, "User Management");
    tabWidget->addTab(ordersTab, "Orders");
    tabWidget->addTab(reportsTab, "Reports");

    setCentralWidget(central);
    setWindowTitle("Admin Panel");
    setMinimumSize(800, 600);
}

void AdminPanel::setupUserTab()
{
    userTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(userTab);
    tableUsers = new QTableWidget();
    layout->addWidget(tableUsers);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    btnBlock = new QPushButton("Block");
    btnUnblock = new QPushButton("Unblock");
    btnDelete = new QPushButton("Delete");
    btnApprove = new QPushButton("Approve Restaurant");
    btnDisapprove = new QPushButton("Disapprove Restaurant");
    btnRefreshUsers = new QPushButton("Refresh");

    buttonLayout->addWidget(btnBlock);
    buttonLayout->addWidget(btnUnblock);
    buttonLayout->addWidget(btnDelete);
    buttonLayout->addWidget(btnApprove);
    buttonLayout->addWidget(btnDisapprove);
    buttonLayout->addWidget(btnRefreshUsers);
    layout->addLayout(buttonLayout);
}

void AdminPanel::setupOrdersTab()
{
    ordersTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(ordersTab);
    tableOrders = new QTableWidget();
    btnRefreshOrders = new QPushButton("Refresh Orders");
    layout->addWidget(tableOrders);
    layout->addWidget(btnRefreshOrders);
}

void AdminPanel::setupReportsTab()
{
    reportsTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(reportsTab);

    // Top section with comboBox and button
    QHBoxLayout* topLayout = new QHBoxLayout();
    comboChartType = new QComboBox();
    comboChartType->addItem("Food sales");
    comboChartType->addItem("Restaurant revenue");
    comboChartType->addItem("More active restaurants");

    btnGenerateChart = new QPushButton("Generate");

    topLayout->addWidget(comboChartType);
    topLayout->addWidget(btnGenerateChart);

    mainLayout->addLayout(topLayout);

    // ChartView to display charts
    chartView = new QChartView();
    chartView->setRenderHint(QPainter::Antialiasing);
    mainLayout->addWidget(chartView);
}

// ================== Connections ==================
void AdminPanel::setupConnections()
{
    // User Management
    connect(btnRefreshUsers, &QPushButton::clicked, this, &AdminPanel::loadUsers);
    connect(btnBlock, &QPushButton::clicked, this, &AdminPanel::blockSelectedUser);
    connect(btnUnblock, &QPushButton::clicked, this, &AdminPanel::unblockSelectedUser);
    connect(btnDelete, &QPushButton::clicked, this, &AdminPanel::deleteSelectedUser);
    connect(btnApprove, &QPushButton::clicked, this, &AdminPanel::approveSelectedRestaurant);
    connect(btnDisapprove, &QPushButton::clicked, this, &AdminPanel::disapproveSelectedRestaurant);

    // Orders
    connect(btnRefreshOrders, &QPushButton::clicked, this, &AdminPanel::loadOrders);

    // Reports
    connect(btnGenerateChart, &QPushButton::clicked, this, &AdminPanel::generateChart);
}

// ================== Helpers ==================

// Get the ID of the selected user in the table
int AdminPanel::getSelectedUserId () const
{
    if (!tableUsers) return -1;
    QModelIndex index = tableUsers->currentIndex();
    if (!index.isValid()) return -1;

    return index.sibling(index.row(), 0).data(Qt::UserRole).toInt();
}

// ================== User Management ==================

// Load users from database into the table
void AdminPanel::loadUsers()
{
    QJsonObject request;
    request["command"] = "get_users";
    m_network->sendJson(request);
}


// Block selected user
void AdminPanel::blockSelectedUser()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a user first!");
        return;
    }

    QJsonObject request;
    request["command"] = "block_user";
    request["user_id"] = userId;
    m_network->sendJson(request);
}



// Unblock selected user
void AdminPanel::unblockSelectedUser()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a user first!");
        return;
    }
    QJsonObject request;
    request["command"] = "unblock_user";
    request["user_id"] = userId;
    m_network->sendJson(request);
}



// Delete selected user
void AdminPanel::deleteSelectedUser()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No user is selected", "Select a user first!");
        return;
    }

    QJsonObject request;
    request["command"] = "delete_user";
    request["user_id"] = userId;
    m_network->sendJson(request);
}



// Approve restaurant
void AdminPanel::approveSelectedRestaurant()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No restaurant is selected", "Please select a restaurant first!");
        return;
    }

    QJsonObject request;
    request["command"] = "approve_restaurant";
    request["user_id"] = userId;
    m_network->sendJson(request);
}



// Disapprove restaurant
void AdminPanel::disapproveSelectedRestaurant()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No restaurant is selected", "Please select a restaurant first!!");
        return;
    }

    QJsonObject request;
    request["command"] = "disapprove_restaurant";
    request["user_id"] = userId;
    m_network->sendJson(request);
}



// ================== Orders ==================

// Load orders from database
void AdminPanel::loadOrders()
{
    QJsonObject request;
    request["command"] = "get_orders";
    m_network->sendJson(request);
}


// ================== Reports ==================

// Handle generate button click
void AdminPanel::generateChart()
{
    QString selected = comboChartType->currentText();

    if (selected == "Food sales") {
        showFoodSalesChart();
    } else if (selected == "Restaurant revenue") {
        showRestaurantRevenueChart();
    } else if (selected == "More active restaurants") {
        showActiveRestaurantsChart();
    }
}

// Show food sales bar chart
void AdminPanel::showFoodSalesChart()
{
    auto *set0 = new QBarSet("Sales");
    *set0 << 100 << 150 << 200;

    auto *series = new QBarSeries();
    series->append(set0);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Food Sales");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    QStringList categories = {"Pizza", "Burger", "Pasta"};
    auto *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis();
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chartView->setChart(chart);
}

// Show restaurant revenue pie chart
void AdminPanel::showRestaurantRevenueChart()
{
    auto *series = new QPieSeries();
    series->append("Restaurant A", 3000);
    series->append("Restaurant B", 1500);
    series->append("Restaurant C", 5000);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Restaurant Revenue");

    chartView->setChart(chart);
}

// Show more active restaurants bar chart
void AdminPanel::showActiveRestaurantsChart()
{
    auto *set0 = new QBarSet("Active Orders");
    *set0 << 40 << 70 << 50;

    auto *series = new QBarSeries();
    series->append(set0);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("More Active Restaurants");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    QStringList categories = {"Restaurant A", "Restaurant B", "Restaurant C"};
    auto *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis();
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chartView->setChart(chart);
}

void AdminPanel::onServerResponse(const QJsonObject& response)
{
    QString command = response["command"].toString();

    // ---------------------- دریافت لیست کاربران ----------------------
    if (command == "get_users") {
        QJsonArray users = response["data"].toArray();

        tableUsers->clearContents();
        tableUsers->setRowCount(0);
        tableUsers->setColumnCount(5);
        tableUsers->setHorizontalHeaderLabels({"ID", "Username", "Role", "Active", "Approved"});

        for (int i = 0; i < users.size(); ++i) {
            QJsonObject u = users[i].toObject();
            tableUsers->insertRow(i);
            tableUsers->setItem(i, 0, new QTableWidgetItem(QString::number(u["id"].toInt())));
            tableUsers->setItem(i, 1, new QTableWidgetItem(u["username"].toString()));
            tableUsers->setItem(i, 2, new QTableWidgetItem(u["role"].toString()));
            tableUsers->setItem(i, 3, new QTableWidgetItem(u["is_active"].toBool() ? "Yes" : "No"));
            tableUsers->setItem(i, 4, new QTableWidgetItem(u["is_approved"].toBool() ? "Yes" : "No"));
        }

        tableUsers->resizeColumnsToContents();
    }

    // ---------------------- دریافت لیست سفارش‌ها ----------------------
    else if (command == "get_orders") {
        QJsonArray orders = response["data"].toArray();

        tableOrders->clearContents();
        tableOrders->setRowCount(0);
        tableOrders->setColumnCount(6);
        tableOrders->setHorizontalHeaderLabels({"ID", "Customer ID", "Restaurant ID", "Status", "Total Price", "Created At"});

        for (int i = 0; i < orders.size(); ++i) {
            QJsonObject o = orders[i].toObject();
            tableOrders->insertRow(i);
            tableOrders->setItem(i, 0, new QTableWidgetItem(QString::number(o["id"].toInt())));
            tableOrders->setItem(i, 1, new QTableWidgetItem(QString::number(o["customer_id"].toInt())));
            tableOrders->setItem(i, 2, new QTableWidgetItem(QString::number(o["restaurant_id"].toInt())));
            tableOrders->setItem(i, 3, new QTableWidgetItem(o["status"].toString()));
            tableOrders->setItem(i, 4, new QTableWidgetItem(QString::number(o["total_price"].toDouble())));
            tableOrders->setItem(i, 5, new QTableWidgetItem(o["created_at"].toString()));
        }

        tableOrders->resizeColumnsToContents();
    }

    // ---------------------- پاسخ حذف کاربر ----------------------
    else if (command == "delete_user") {
        if (response["success"].toBool()) {
            QMessageBox::information(this, "حذف شد", "کاربر با موفقیت حذف شد.");
            loadUsers(); // رفرش جدول کاربران
        } else {
            QMessageBox::warning(this, "خطا", "خطا در حذف کاربر.");
        }
    }

    // ---------------------- پاسخ بلاک / آنبلاک ----------------------
    else if (command == "block_user") {
        QMessageBox::information(this, "نتیجه", response["success"].toBool() ? "کاربر مسدود شد." : "عملیات مسدودسازی شکست خورد.");
        loadUsers();
    }
    else if (command == "unblock_user") {
        QMessageBox::information(this, "نتیجه", response["success"].toBool() ? "کاربر آزاد شد." : "عملیات آزادسازی شکست خورد.");
        loadUsers();
    }

    // ---------------------- تأیید / رد رستوران ----------------------
    else if (command == "approve_restaurant") {
        QMessageBox::information(this, "نتیجه", response["success"].toBool() ? "رستوران تأیید شد." : "خطا در تأیید رستوران.");
        loadUsers();
    }
    else if (command == "disapprove_restaurant") {
        QMessageBox::information(this, "نتیجه", response["success"].toBool() ? "رستوران رد شد." : "خطا در رد رستوران.");
        loadUsers();
    }

    // ---------------------- پاسخ‌های ناشناخته ----------------------
    else {
        qDebug() << "Unknown server response:" << response;
    }
}

