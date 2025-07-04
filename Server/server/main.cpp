#include <QCoreApplication>
#include <QDebug>
#include "server.h"
#include "../../../IUT_Food/databasehandler.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "Starting server...";

    // ساخت هندلر دیتابیس
    DataBaseHandler dbHandler;
    if (!dbHandler.openDataBase("server_database.sqlite")) {
        qCritical() << "❌ Failed to open database!";
        return -1;
    }

    if (!dbHandler.createTables()) {
        qCritical() << "❌ Failed to create tables!";
        return -1;
    }

    // ساخت و راه‌اندازی سرور
    Server server(&dbHandler);
    if (!server.listen(QHostAddress::Any, 1234)) {
        qCritical() << "❌ Server failed to start on port 1234!";
        return -1;
    }

    qDebug() << "✅ Server is listening on port 1234.";

    return app.exec();
}
