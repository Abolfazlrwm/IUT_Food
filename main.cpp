#include "client.h"
#include "databasehandler.h"
#include "networkmanager.h"
#include <QApplication>
#include <QMessageBox> // <<< هدر لازم برای نمایش پیغام
#include "adminpanel.h"
#include <QDir>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NetworkManager::getInstance()->connectToServer("127.0.0.1", 1234); // IP & PORT
    // ۱. مسیر دقیق کنار فایل اجرایی برنامه را پیدا می‌کنیم
    QString dbPath = QCoreApplication::applicationDirPath() + "/iut_food_data.db";

    DataBaseHandler dbHandler;
    // ۲. دیتابیس را با مسیر کامل و دقیق باز می‌کنیم
    if (!dbHandler.openDataBase(dbPath)) {
        QMessageBox::critical(nullptr, "خطای دیتابیس",
                              QString("برنامه قادر به اتصال به پایگاه داده در مسیر زیر نیست:\n%1").arg(dbPath));
        return -1;
    }
    dbHandler.createTables();

    // === نمایش آدرس دیتابیس در یک پیغام ساده ===
    QMessageBox::information(nullptr, "آدرس دیتابیس",
                             QString("برنامه با موفقیت به دیتابیس زیر متصل شد:\n%1").arg(QDir(dbPath).absolutePath()));
    // ==========================================

    // اتصال به سرور
    NetworkManager::getInstance()->connectToServer("127.0.0.1", 1234);

    // ساخت و نمایش پنجره اصلی
    //Client w(&dbHandler);
    AdminPanel w;
    w.show();

    return a.exec();
}
