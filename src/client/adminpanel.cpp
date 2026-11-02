#include "adminpanel.h"

#include "networkmanager.h"
#include "orderstatus.h"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPieSeries>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>

AdminPanel::AdminPanel(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    setupConnections();

    // Network manager BEFORE any data request is issued (the original
    // build called loadUsers() through an uninitialised pointer).
    m_network = &NetworkManager::instance();
    connect(m_network, &NetworkManager::jsonReceived, this, &AdminPanel::onServerResponse);
    connect(m_network, &NetworkManager::connected, this, [this]() {
        m_connectionLabel->setText(QStringLiteral("🟢 متصل به سرور"));
        loadUsers();
        loadOrders();
        loadReviews();
    });
    connect(m_network, &NetworkManager::disconnected, this, [this]() {
        m_connectionLabel->setText(QStringLiteral("🔴 اتصال قطع است — سرور را اجرا کنید"));
    });

    m_network->connectToServer(); // port 1234, matching the server default
    // The shared singleton may already be connected (the customer window
    // connects too) - the connected() signal would then never fire again.
    if (m_network->isConnected()) {
        m_connectionLabel->setText(QStringLiteral("🟢 متصل به سرور"));
        loadUsers();
        loadOrders();
        loadReviews();
    }
}

AdminPanel::~AdminPanel() = default;

void AdminPanel::setupUI()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    m_connectionLabel = new QLabel(QStringLiteral("در حال اتصال…"), this);
    m_connectionLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_connectionLabel);

    tabWidget = new QTabWidget(central);
    mainLayout->addWidget(tabWidget);

    setupUserTab();
    setupOrdersTab();
    setupReportsTab();

    tabWidget->addTab(userTab, QStringLiteral("مدیریت کاربران"));
    tabWidget->addTab(ordersTab, QStringLiteral("سفارش‌ها"));
    tabWidget->addTab(reportsTab, QStringLiteral("گزارش‌ها"));

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("IUT-Food — پنل مدیریت"));
    setLayoutDirection(Qt::RightToLeft);
    setMinimumSize(880, 620);

    this->setStyleSheet(QStringLiteral(R"(
    QWidget { font-family: "Vazirmatn", "Segoe UI", sans-serif; font-size: 13px; background-color: #f4f6f8; color: #333; }
    QTableWidget { border: 1px solid #ccc; border-radius: 6px; gridline-color: #ddd; selection-background-color: #cce5ff; background: white; }
    QPushButton { background-color: #0078d7; color: white; border: none; border-radius: 6px; padding: 8px 14px; }
    QPushButton:hover { background-color: #005fa3; }
    QPushButton:disabled { background-color: #cccccc; color: #666666; }
    QTabWidget::pane { border: 1px solid #aaa; border-radius: 8px; }
    QTabBar::tab { background: #e0e0e0; border: 1px solid #ccc; border-bottom: none; padding: 8px 18px; border-top-left-radius: 8px; border-top-right-radius: 8px; margin-right: 2px; }
    QTabBar::tab:selected { background: #ffffff; font-weight: bold; }
)"));
}

void AdminPanel::setupUserTab()
{
    userTab = new QWidget();
    auto *layout = new QVBoxLayout(userTab);
    tableUsers = new QTableWidget();
    tableUsers->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableUsers->setSelectionMode(QAbstractItemView::SingleSelection);
    tableUsers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(tableUsers);

    auto *buttonLayout = new QHBoxLayout();
    btnBlock = new QPushButton(QStringLiteral("مسدودسازی"));
    btnUnblock = new QPushButton(QStringLiteral("رفع مسدودی"));
    btnDelete = new QPushButton(QStringLiteral("حذف کاربر"));
    btnApprove = new QPushButton(QStringLiteral("تأیید رستوران"));
    btnDisapprove = new QPushButton(QStringLiteral("رد رستوران"));
    btnRefreshUsers = new QPushButton(QStringLiteral("به‌روزرسانی"));

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
    auto *layout = new QVBoxLayout(ordersTab);
    tableOrders = new QTableWidget();
    tableOrders->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableOrders->setEditTriggers(QAbstractItemView::NoEditTriggers);
    btnRefreshOrders = new QPushButton(QStringLiteral("به‌روزرسانی سفارش‌ها"));
    layout->addWidget(tableOrders);
    layout->addWidget(btnRefreshOrders);
}

void AdminPanel::setupReportsTab()
{
    reportsTab = new QWidget();
    auto *mainLayout = new QVBoxLayout(reportsTab);

    auto *topLayout = new QHBoxLayout();
    comboChartType = new QComboBox();
    comboChartType->addItems({
        QStringLiteral("وضعیت سفارش‌ها"),
        QStringLiteral("درآمد رستوران‌ها"),
        QStringLiteral("پرکاربردترین رستوران‌ها"),
        QStringLiteral("میانگین امتیاز رستوران‌ها"),
    });
    btnGenerateChart = new QPushButton(QStringLiteral("نمایش نمودار"));
    topLayout->addWidget(comboChartType);
    topLayout->addWidget(btnGenerateChart);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    chartView = new QChartView();
    chartView->setRenderHint(QPainter::Antialiasing);
    mainLayout->addWidget(chartView);
}

void AdminPanel::setupConnections()
{
    connect(btnRefreshUsers, &QPushButton::clicked, this, &AdminPanel::loadUsers);
    connect(btnBlock, &QPushButton::clicked, this, &AdminPanel::blockSelectedUser);
    connect(btnUnblock, &QPushButton::clicked, this, &AdminPanel::unblockSelectedUser);
    connect(btnDelete, &QPushButton::clicked, this, &AdminPanel::deleteSelectedUser);
    connect(btnApprove, &QPushButton::clicked, this, &AdminPanel::approveSelectedRestaurant);
    connect(btnDisapprove, &QPushButton::clicked, this, &AdminPanel::disapproveSelectedRestaurant);
    connect(btnRefreshOrders, &QPushButton::clicked, this, &AdminPanel::loadOrders);
    connect(btnGenerateChart, &QPushButton::clicked, this, &AdminPanel::generateChart);
}

int AdminPanel::getSelectedUserId() const
{
    if (!tableUsers)
        return -1;
    const QModelIndex index = tableUsers->currentIndex();
    if (!index.isValid())
        return -1;
    // The id now travels in UserRole of column 0 (it was never stored in
    // the original build, so every action hit user_id = 0).
    return index.sibling(index.row(), 0).data(Qt::UserRole).toInt();
}

// ============================ user actions ============================

void AdminPanel::loadUsers()
{
    m_network->sendJson(QJsonObject{{QStringLiteral("command"), QStringLiteral("get_users")}});
}

void AdminPanel::loadOrders()
{
    m_network->sendJson(QJsonObject{{QStringLiteral("command"), QStringLiteral("get_orders")}});
}

void AdminPanel::loadReviews()
{
    m_network->sendJson(QJsonObject{{QStringLiteral("command"), QStringLiteral("get_reviews")}});
}

void AdminPanel::blockSelectedUser()
{
    const int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, QStringLiteral("انتخابی نیست"), QStringLiteral("ابتدا یک کاربر را انتخاب کنید!"));
        return;
    }
    m_network->sendJson(QJsonObject{{QStringLiteral("command"), QStringLiteral("block_user")},
                                    {QStringLiteral("user_id"), userId}});
}

void AdminPanel::unblockSelectedUser()
{
    const int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, QStringLiteral("انتخابی نیست"), QStringLiteral("ابتدا یک کاربر را انتخاب کنید!"));
        return;
    }
    m_network->sendJson(QJsonObject{{QStringLiteral("command"), QStringLiteral("unblock_user")},
                                    {QStringLiteral("user_id"), userId}});
}

void AdminPanel::deleteSelectedUser()
{
    const int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, QStringLiteral("انتخابی نیست"), QStringLiteral("ابتدا یک کاربر را انتخاب کنید!"));
        return;
    }
    m_network->sendJson(QJsonObject{{QStringLiteral("command"), QStringLiteral("delete_user")},
                                    {QStringLiteral("user_id"), userId}});
}

void AdminPanel::approveSelectedRestaurant()
{
    const int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, QStringLiteral("انتخابی نیست"), QStringLiteral("ابتدا یک رستوران را انتخاب کنید!"));
        return;
    }
    m_network->sendJson(QJsonObject{{QStringLiteral("command"), QStringLiteral("approve_restaurant")},
                                    {QStringLiteral("user_id"), userId}});
}

void AdminPanel::disapproveSelectedRestaurant()
{
    const int userId = getSelectedUserId();
    if (userId < 0) {
        QMessageBox::warning(this, QStringLiteral("انتخابی نیست"), QStringLiteral("ابتدا یک رستوران را انتخاب کنید!"));
        return;
    }
    m_network->sendJson(QJsonObject{{QStringLiteral("command"), QStringLiteral("disapprove_restaurant")},
                                    {QStringLiteral("user_id"), userId}});
}

// ============================ server data ============================

void AdminPanel::onServerResponse(const QJsonObject &response)
{
    const QString command = response.value(QLatin1String("command")).toString();

    if (command == QLatin1String("get_users")) {
        m_lastUsers = response;
        const QJsonArray users = response.value(QLatin1String("data")).toArray();

        tableUsers->clearContents();
        tableUsers->setRowCount(0);
        tableUsers->setColumnCount(5);
        tableUsers->setHorizontalHeaderLabels({QStringLiteral("شناسه"), QStringLiteral("نام کاربری"),
                                               QStringLiteral("نقش"), QStringLiteral("فعال"),
                                               QStringLiteral("تأیید شده")});

        for (int i = 0; i < users.size(); ++i) {
            const QJsonObject u = users[i].toObject();
            tableUsers->insertRow(i);
            auto *idItem = new QTableWidgetItem(QString::number(u.value(QLatin1String("id")).toInt()));
            idItem->setData(Qt::UserRole, u.value(QLatin1String("id")).toInt());
            tableUsers->setItem(i, 0, idItem);
            tableUsers->setItem(i, 1, new QTableWidgetItem(u.value(QLatin1String("username")).toString()));
            tableUsers->setItem(i, 2, new QTableWidgetItem(u.value(QLatin1String("role")).toString()));
            tableUsers->setItem(i, 3, new QTableWidgetItem(
                u.value(QLatin1String("is_active")).toBool() ? QStringLiteral("بله") : QStringLiteral("خیر")));
            tableUsers->setItem(i, 4, new QTableWidgetItem(
                u.value(QLatin1String("is_approved")).toBool() ? QStringLiteral("بله") : QStringLiteral("خیر")));
        }
        tableUsers->resizeColumnsToContents();
    }

    else if (command == QLatin1String("get_orders")) {
        m_lastOrders = response;
        const QJsonArray orders = response.value(QLatin1String("data")).toArray();

        tableOrders->clearContents();
        tableOrders->setRowCount(0);
        tableOrders->setColumnCount(6);
        tableOrders->setHorizontalHeaderLabels({QStringLiteral("شناسه"), QStringLiteral("مشتری"),
                                                QStringLiteral("رستوران"), QStringLiteral("وضعیت"),
                                                QStringLiteral("مبلغ (تومان)"), QStringLiteral("زمان")});

        // id -> username lookup for readable customer/restaurant columns.
        QHash<int, QString> names;
        const QJsonArray users = m_lastUsers.value(QLatin1String("data")).toArray();
        for (const QJsonValue &u : users) {
            const QJsonObject obj = u.toObject();
            names.insert(obj.value(QLatin1String("id")).toInt(),
                         obj.value(QLatin1String("username")).toString());
        }

        for (int i = 0; i < orders.size(); ++i) {
            const QJsonObject o = orders[i].toObject();
            const int customerId = o.value(QLatin1String("customer_id")).toInt();
            const int restaurantId = o.value(QLatin1String("restaurant_id")).toInt();
            tableOrders->insertRow(i);
            tableOrders->setItem(i, 0, new QTableWidgetItem(QString::number(o.value(QLatin1String("id")).toInt())));
            tableOrders->setItem(i, 1, new QTableWidgetItem(
                names.value(customerId, QStringLiteral("#") + QString::number(customerId))));
            tableOrders->setItem(i, 2, new QTableWidgetItem(
                names.value(restaurantId, QStringLiteral("#") + QString::number(restaurantId))));
            tableOrders->setItem(i, 3, new QTableWidgetItem(
                OrderStatus::toPersian(o.value(QLatin1String("status")).toString())));
            tableOrders->setItem(i, 4, new QTableWidgetItem(
                QString::number(o.value(QLatin1String("total_price")).toDouble())));
            tableOrders->setItem(i, 5, new QTableWidgetItem(o.value(QLatin1String("created_at")).toString()));
        }
        tableOrders->resizeColumnsToContents();
    }

    else if (command == QLatin1String("get_reviews")) {
        m_lastReviews = response;
    }

    else if (command == QLatin1String("delete_user")) {
        if (response.value(QLatin1String("success")).toBool()) {
            QMessageBox::information(this, QStringLiteral("حذف شد"), QStringLiteral("کاربر با موفقیت حذف شد."));
            loadUsers();
        } else {
            QMessageBox::warning(this, QStringLiteral("خطا"), QStringLiteral("خطا در حذف کاربر."));
        }
    }

    else if (command == QLatin1String("block_user")) {
        QMessageBox::information(this, QStringLiteral("نتیجه"),
            response.value(QLatin1String("success")).toBool()
                ? QStringLiteral("کاربر مسدود شد.") : QStringLiteral("عملیات مسدودسازی ناموفق بود."));
        loadUsers();
    }
    else if (command == QLatin1String("unblock_user")) {
        QMessageBox::information(this, QStringLiteral("نتیجه"),
            response.value(QLatin1String("success")).toBool()
                ? QStringLiteral("کاربر آزاد شد.") : QStringLiteral("عملیات آزادسازی ناموفق بود."));
        loadUsers();
    }
    else if (command == QLatin1String("approve_restaurant")) {
        QMessageBox::information(this, QStringLiteral("نتیجه"),
            response.value(QLatin1String("success")).toBool()
                ? QStringLiteral("رستوران تأیید شد.") : QStringLiteral("خطا در تأیید رستوران."));
        loadUsers();
    }
    else if (command == QLatin1String("disapprove_restaurant")) {
        QMessageBox::information(this, QStringLiteral("نتیجه"),
            response.value(QLatin1String("success")).toBool()
                ? QStringLiteral("رستوران رد شد.") : QStringLiteral("خطا در رد رستوران."));
        loadUsers();
    }
}

// ============================== charts ================================

void AdminPanel::generateChart()
{
    // All four reports are computed from the freshest server data.
    loadUsers();
    loadOrders();
    loadReviews();

    const QString selected = comboChartType->currentText();
    if (selected == QStringLiteral("وضعیت سفارش‌ها"))
        showOrdersByStatusChart();
    else if (selected == QStringLiteral("درآمد رستوران‌ها"))
        showRestaurantRevenueChart();
    else if (selected == QStringLiteral("پرکاربردترین رستوران‌ها"))
        showActiveRestaurantsChart();
    else
        showAverageRatingChart();
}

void AdminPanel::showOrdersByStatusChart()
{
    // Count orders per canonical status from the live server data instead
    // of the hard-coded numbers the original build displayed.
    QHash<QString, int> counts;
    const QJsonArray orders = m_lastOrders.value(QLatin1String("data")).toArray();
    for (const QJsonValue &v : orders) {
        const QString status = v.toObject().value(QLatin1String("status")).toString();
        counts[status] += 1;
    }
    if (counts.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("گزارش"),
                                 QStringLiteral("هنوز سفارشی ثبت نشده است."));
        return;
    }

    auto *set = new QBarSet(QStringLiteral("تعداد سفارش"));
    QStringList categories;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        categories << OrderStatus::toPersian(it.key());
        *set << it.value();
    }

    auto *series = new QBarSeries();
    series->append(set);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("وضعیت سفارش‌ها (داده‌های زنده سرور)"));
    chart->setAnimationOptions(QChart::SeriesAnimations);

    auto *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis();
    axisY->setLabelFormat(QStringLiteral("%d"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->legend()->hide();

    chartView->setChart(chart);
}

void AdminPanel::showRestaurantRevenueChart()
{
    const QJsonArray orders = m_lastOrders.value(QLatin1String("data")).toArray();
    const QJsonArray users = m_lastUsers.value(QLatin1String("data")).toArray();

    QHash<int, double> revenue;
    for (const QJsonValue &v : orders) {
        const QJsonObject o = v.toObject();
        revenue[o.value(QLatin1String("restaurant_id")).toInt()] +=
            o.value(QLatin1String("total_price")).toDouble();
    }
    QHash<int, QString> names;
    for (const QJsonValue &u : users) {
        const QJsonObject obj = u.toObject();
        if (obj.value(QLatin1String("role")).toString() == QLatin1String("restaurant"))
            names.insert(obj.value(QLatin1String("id")).toInt(),
                         obj.value(QLatin1String("username")).toString());
    }
    if (revenue.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("گزارش"),
                                 QStringLiteral("هنوز سفارشی ثبت نشده است."));
        return;
    }

    auto *series = new QPieSeries();
    for (auto it = revenue.cbegin(); it != revenue.cend(); ++it)
        series->append(names.value(it.key(), QStringLiteral("رستوران #%1").arg(it.key())),
                       it.value());

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("درآمد رستوران‌ها (تومان)"));
    chartView->setChart(chart);
}

void AdminPanel::showActiveRestaurantsChart()
{
    const QJsonArray orders = m_lastOrders.value(QLatin1String("data")).toArray();
    const QJsonArray users = m_lastUsers.value(QLatin1String("data")).toArray();

    QHash<int, int> orderCount;
    for (const QJsonValue &v : orders)
        orderCount[v.toObject().value(QLatin1String("restaurant_id")).toInt()] += 1;

    QHash<int, QString> names;
    for (const QJsonValue &u : users) {
        const QJsonObject obj = u.toObject();
        if (obj.value(QLatin1String("role")).toString() == QLatin1String("restaurant"))
            names.insert(obj.value(QLatin1String("id")).toInt(),
                         obj.value(QLatin1String("username")).toString());
    }
    if (orderCount.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("گزارش"),
                                 QStringLiteral("هنوز سفارشی ثبت نشده است."));
        return;
    }

    auto *set = new QBarSet(QStringLiteral("تعداد سفارش"));
    QStringList categories;
    for (auto it = orderCount.cbegin(); it != orderCount.cend(); ++it) {
        categories << names.value(it.key(), QStringLiteral("رستوران #%1").arg(it.key()));
        *set << it.value();
    }

    auto *series = new QBarSeries();
    series->append(set);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("پرکاربردترین رستوران‌ها"));
    chart->setAnimationOptions(QChart::SeriesAnimations);

    auto *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis();
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->legend()->hide();

    chartView->setChart(chart);
}

void AdminPanel::showAverageRatingChart()
{
    const QJsonArray reviews = m_lastReviews.value(QLatin1String("data")).toArray();
    if (reviews.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("گزارش"),
                                 QStringLiteral("هنوز نظری ثبت نشده است."));
        return;
    }

    QHash<QString, QPair<int, int>> ratingPerRestaurant; // name -> (sum, count)
    for (const QJsonValue &v : reviews) {
        const QJsonObject r = v.toObject();
        const QString name = r.value(QLatin1String("restaurant_name")).toString();
        auto &entry = ratingPerRestaurant[name];
        entry.first += r.value(QLatin1String("rating")).toInt();
        entry.second += 1;
    }

    auto *set = new QBarSet(QStringLiteral("میانگین امتیاز (از ۵)"));
    QStringList categories;
    for (auto it = ratingPerRestaurant.cbegin(); it != ratingPerRestaurant.cend(); ++it) {
        categories << it.key();
        *set << (double(it.value().first) / it.value().second);
    }

    auto *series = new QBarSeries();
    series->append(set);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("میانگین امتیاز مشتریان از رستوران‌ها"));
    chart->setAnimationOptions(QChart::SeriesAnimations);

    auto *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis();
    axisY->setRange(0, 5);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    chart->legend()->hide();

    chartView->setChart(chart);
}
