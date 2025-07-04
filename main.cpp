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

    // ساخت و نمایش پنجره اصلی
    Client w(&dbHandler);
    //AdminPanel w;
    w.show();

    return a.exec();
}
// #include "client.h" // یا هر کلاسی که پنجره اصلی شماست
// #include "databasehandler.h"
// #include <QApplication>

// int main(int argc, char *argv[])
// {
//     QApplication a(argc, argv);

//     // فرض می‌کنیم دیتابیس هندلر اینجا ساخته می‌شود
//     DataBaseHandler dbHandler;
//     dbHandler.openDataBase("food_data.db");
//     // dbHandler.createTables(); // فقط برای اولین بار
//     NetworkManager::getInstance()->connectToServer("127.0.0.1", 1234);

//     Client w(&dbHandler); // اشاره‌گر dbHandler به Client پاس داده می‌شود
//     w.show();

//     // این خط حیاتی است! برنامه را در حلقه رویداد نگه می‌دارد و از بسته شدن آن جلوگیری می‌کند
//     return a.exec();
// }
