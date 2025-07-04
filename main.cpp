#include "client.h"
#include "databasehandler.h"
#include "networkmanager.h"
#include <QApplication>
#include <QMessageBox>
#include "adminpanel.h"
#include "restaurant.h"
#include <QDir>
#include <QDebug>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NetworkManager::getInstance()->connectToServer("127.0.0.1", 1234); // IP & PORT

    // ۱. مسیر دقیق کنار فایل اجرایی برنامه را پیدا می‌کنیم
    QString dbPath = QCoreApplication::applicationDirPath() + "/iut_food_data.db";

    // از singleton دیتابیس هندلر استفاده می‌کنیم
    DataBaseHandler& dbHandler = DataBaseHandler::instance();

    // ۲. دیتابیس را با مسیر کامل و دقیق باز می‌کنیم
    if (!dbHandler.openDataBase(dbPath)) {
        QMessageBox::critical(nullptr, "خطای دیتابیس",
                              QString("برنامه قادر به اتصال به پایگاه داده در مسیر زیر نیست:\n%1").arg(dbPath));
        return -1;
    }
    dbHandler.createTables();

    // === نمایش آدرس دیتابیس در یک پیغام ساده ===
    MainWindow w;
    //AdminPanel w;
    w.show();


    return a.exec();
}
