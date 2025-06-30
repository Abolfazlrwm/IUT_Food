#include "AdminPanel.h"

// ================== Constructor ==================
AdminPanel::AdminPanel(DataBaseHandler* handler, QWidget *parent)
    : QMainWindow(parent), dbHandler(handler)
{
    setupUI();          // Build all tabs and UI
    setupConnections(); // Connect buttons to slots
    loadUsers();        // Load initial user data
    loadOrders();       // Load initial orders
}

AdminPanel::~AdminPanel() {}

// ================== UI SETUP ==================
void AdminPanel::setupUI()
{
    // Central widget and main layout
    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // Main QTabWidget
    tabWidget = new QTabWidget(central);
    mainLayout->addWidget(tabWidget);

    // Build individual tabs
    setupUserTab();
    setupOrdersTab();
    setupReportsTab();

    // Add tabs to tabWidget
    tabWidget->addTab(userTab, "User Management");
    tabWidget->addTab(ordersTab, "Orders");
    tabWidget->addTab(reportsTab, "Reports");

    setCentralWidget(central);
}

void AdminPanel::setupUserTab()
{
    userTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(userTab);

    // User table
    tableUsers = new QTableWidget();
    layout->addWidget(tableUsers);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    btnBlock = new QPushButton("Block");
    btnUnblock = new QPushButton("Unblock");
    btnDelete = new QPushButton("Delete");
    btnApprove = new QPushButton("Approve Restaurant");
    btnDisapprove = new QPushButton("Disapprove Restaurant");
    btnRefreshUsers = new QPushButton("Refresh");

    // Add buttons to layout
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

    // Orders table and refresh button
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
int AdminPanel::getSelectedUserId() const
{
    int row = tableUsers->currentRow();
    if (row < 0) return -1;
    return tableUsers->item(row, 0)->text().toInt();
}

// ================== User Management ==================

// Load users from database into the table
void AdminPanel::loadUsers()
{
    tableUsers->clearContents();
    tableUsers->setRowCount(0);

    QStringList headers;
    headers << "ID" << "Username" << "Role" << "is_active" << "is_approved";
    tableUsers->setColumnCount(headers.size());
    tableUsers->setHorizontalHeaderLabels(headers);

    QSqlQuery q = dbHandler->readAllUsers();
    int row = 0;

    while (q.next()) {
        tableUsers->insertRow(row);
        tableUsers->setItem(row, 0, new QTableWidgetItem(q.value("id").toString()));
        tableUsers->setItem(row, 1, new QTableWidgetItem(q.value("username").toString()));
        tableUsers->setItem(row, 2, new QTableWidgetItem(q.value("role").toString()));
        tableUsers->setItem(row, 3, new QTableWidgetItem(q.value("is_active").toString()));
        tableUsers->setItem(row, 4, new QTableWidgetItem(q.value("is_approved").toString()));
        row++;
    }
    tableUsers->resizeColumnsToContents();
}

// Block selected user
void AdminPanel::blockSelectedUser()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a user first!");
        return;
    }
    if (dbHandler->blockUser(userId)) {
        QMessageBox::information(this, "Success", "User blocked successfully.");
        loadUsers();
    }
}

// Unblock selected user
void AdminPanel::unblockSelectedUser()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a user first!");
        return;
    }
    if (dbHandler->unblockUser(userId)) {
        QMessageBox::information(this, "Success", "User unblocked successfully.");
        loadUsers();
    }
}

// Delete selected user
void AdminPanel::deleteSelectedUser()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a user first!");
        return;
    }
    if (dbHandler->deleteUser(userId)) {
        QMessageBox::information(this, "Deleted", "User deleted successfully.");
        loadUsers();
    }
}

// Approve restaurant
void AdminPanel::approveSelectedRestaurant()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a restaurant first!");
        return;
    }
    if (dbHandler->approveRestaurant(userId)) {
        QMessageBox::information(this, "Approved", "Restaurant approved successfully.");
        loadUsers();
    }
}

// Disapprove restaurant
void AdminPanel::disapproveSelectedRestaurant()
{
    int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a restaurant first!");
        return;
    }
    if (dbHandler->disapproveRestaurant(userId)) {
        QMessageBox::information(this, "Disapproved", "Restaurant disapproved successfully.");
        loadUsers();
    }
}

// ================== Orders ==================

// Load orders from database
void AdminPanel::loadOrders()
{
    tableOrders->clearContents();
    tableOrders->setRowCount(0);

    QStringList headers;
    headers << "id" << "customer_id" << "restaurant_id" << "status" << "total_price" << "created_at";
    tableOrders->setColumnCount(headers.size());
    tableOrders->setHorizontalHeaderLabels(headers);

    QSqlQuery q = dbHandler->readAllOrders();
    int row = 0;

    while (q.next()) {
        tableOrders->insertRow(row);
        tableOrders->setItem(row, 0, new QTableWidgetItem(q.value("id").toString()));
        tableOrders->setItem(row, 1, new QTableWidgetItem(q.value("customer_id").toString()));
        tableOrders->setItem(row, 2, new QTableWidgetItem(q.value("restaurant_id").toString()));
        tableOrders->setItem(row, 3, new QTableWidgetItem(q.value("status").toString()));
        tableOrders->setItem(row, 4, new QTableWidgetItem(q.value("total_price").toString()));
        tableOrders->setItem(row, 5, new QTableWidgetItem(q.value("created_at").toString()));
        row++;
    }
    tableOrders->resizeColumnsToContents();
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
