#include "client.h"
#include "databasehandler.h"
#include <QApplication>
#include <QMessageBox>
#include <QDir>        // <<< هدر جدید برای کار با مسیرها
#include <QDebug>      // <<< هدر جدید برای چاپ پیام دیباگ

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ... (کد تنظیم فونت اگر دارید) ...

    // ۱. مسیر دقیق کنار فایل اجرایی برنامه را پیدا می‌کنیم
    QString dbPath = QCoreApplication::applicationDirPath() + "/iut_food_data.db";

    // ۲. این مسیر را در خروجی چاپ می‌کنیم تا دقیقا بدانید فایل کجاست
    qDebug() << "Database file is expected at:" << dbPath;

    DataBaseHandler dbHandler;
    // ۳. دیتابیس را با مسیر کامل و دقیق باز می‌کنیم
    if (!dbHandler.openDataBase(dbPath)) {
        QMessageBox::critical(nullptr, "Database Error", "Could not connect to the database.");
        return -1;
    }
    dbHandler.createTables();

    Client w(&dbHandler);
    w.show();

    return a.exec();
}
