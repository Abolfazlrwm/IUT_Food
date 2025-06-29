#include "client.h"
#include "databasehandler.h" // <<<
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ۱. ساخت یک نمونه از مدیر دیتابیس
    DataBaseHandler dbHandler;

    // ۲. باز کردن و ساخت جداول
    if (!dbHandler.openDataBase("iut_food.db")) {
        QMessageBox::critical(nullptr, "خطای دیتابیس", "برنامه قادر به اتصال به پایگاه داده نیست.");
        return -1;
    }
    dbHandler.createTables();

    // ۳. پاس دادن اشاره‌گر دیتابیس به پنجره اصلی
    Client w(&dbHandler);
    w.show();

    return a.exec();
}
