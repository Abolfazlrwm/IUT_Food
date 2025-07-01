#include "client.h"
#include "databasehandler.h"
#include "networkmanager.h" // برای دسترسی به مدیر شبکه
#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ۱. آماده‌سازی دیتابیس محلی
    QString dbPath = QCoreApplication::applicationDirPath() + "/iut_food_data.db";
    qDebug() << "Database file is expected at:" << dbPath;
    DataBaseHandler dbHandler;
    if (!dbHandler.openDataBase(dbPath)) {
        QMessageBox::critical(nullptr, "Database Error", "Could not connect to the database.");
        return -1;
    }
    dbHandler.createTables();

    // ۲. اتصال به سرور در ابتدای برنامه
    NetworkManager::getInstance()->connectToServer("127.0.0.1", 1234);

    // ۳. ساخت و نمایش پنجره اصلی
    Client w(&dbHandler);
    w.show();

    return a.exec();
}
