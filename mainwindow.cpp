#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "client.h"
#include "adminpanel.h"
#include "restaurantwindow.h"
#include "databasehandler.h"
#include "restaurant.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}




void MainWindow::on_userButton_clicked()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/iut_food_data.db";
    DataBaseHandler& dbHandler = DataBaseHandler::instance();

    if (!dbHandler.openDataBase(dbPath)) {
        QMessageBox::critical(nullptr, "خطای دیتابیس",
                              QString("برنامه قادر به اتصال به پایگاه داده در مسیر زیر نیست:\n%1").arg(dbPath));
        return;
    }
    dbHandler.createTables();

    if (!clientWindow) // فقط اگر ساخته نشده
        clientWindow = new Client(&dbHandler);
    clientWindow->show();
    clientWindow->raise();
    clientWindow->activateWindow();
}

void MainWindow::on_adminButton_clicked()
{
    if (!adminPanelWindow) {
        adminPanelWindow = new AdminPanel(this);
        connect(adminPanelWindow, &QObject::destroyed, [this]() {
            adminPanelWindow = nullptr;
        });
    }
    adminPanelWindow->show();
    adminPanelWindow->raise();
    adminPanelWindow->activateWindow();
}
void MainWindow::on_restaurantButton_clicked()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/restaurant_management.db";
    if (!DataBaseHandler::instance().openDataBase(dbPath)) {
        QMessageBox::critical(nullptr, "خطا", "امکان اتصال به پایگاه داده وجود ندارد!");
        return;
    }

    QString restaurantUsername = "test_restaurant";
    DataBaseHandler::instance().registerRestaurant(restaurantUsername, "123", "رستوران تست", "فست فود", "تهران", 1);
    int restaurantId = DataBaseHandler::instance().getUserId(restaurantUsername);
    DataBaseHandler::instance().approveRestaurant(restaurantId);

    // 2. ایجاد یک مشتری تستی (برای ثبت سفارش لازم است)
    QString customerUsername = "test_customer";
    // (از یک تابع عمومی ثبت‌نام کاربر استفاده می‌کنیم یا تابع registerRestaurant را کمی تغییر می‌دهیم)
    // برای سادگی، فرض می‌کنیم یک کاربر مشتری هم در جدول users ثبت می‌کنیم.
    QSqlQuery q;
    q.prepare("INSERT INTO users (username, password, role, name, is_approved) VALUES (?, ?, 'customer', ?, 1)");
    q.addBindValue(customerUsername);
    q.addBindValue("123");
    q.addBindValue("مشتری تستی");
    q.exec();
    int customerId = DataBaseHandler::instance().getUserId(customerUsername);

    // 3. ایجاد یک سفارش تستی برای رستوران (با وضعیت "pending")
    QVector<QPair<int, int>> foodList = {
        {1, 2}, // food_id=1, quantity=2
        {2, 1}  // food_id=2, quantity=1
    };
    DataBaseHandler::instance().createTestOrder(restaurantId, customerId, foodList);


    DataBaseHandler::instance().createTestOrder(1, 2, foodList);

    // --- پایان راه‌اندازی داده‌های تستی ---
    Restaurant* restaurant = new Restaurant(restaurantId, restaurantUsername);

    if (!restaurantWindow)
        restaurantWindow = new RestaurantWindow(restaurant);
    restaurantWindow->show();
    restaurantWindow->raise();
    restaurantWindow->activateWindow();
}

