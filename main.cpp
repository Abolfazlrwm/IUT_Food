#include "client.h"
#include "databasehandler.h"
#include "networkmanager.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);




    // ۱. ساخت مدیر دیتابیس
    DataBaseHandler dbHandler;
    if (!dbHandler.openDataBase("iut_food_client.db")) {
        QMessageBox::critical(nullptr, "Database Error", "Could not connect to the database.");
        return -1;
    }
    dbHandler.createTables();

    // ۲. اتصال به سرور
    NetworkManager::getInstance()->connectToServer();

    // ۳. ساخت پنجره اصلی و پاس دادن اشاره‌گر دیتابیس به آن
    Client w(&dbHandler);
    w.show();

    return a.exec();
}
