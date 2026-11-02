#include <QCoreApplication>
#include <QCommandLineParser>
#include <QHostAddress>
#include <QDebug>

#include "databasehandler.h"
#include "demodata.h"
#include "server.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("IUT-Food Server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "TCP server for the IUT-Food ordering system (newline-delimited JSON)."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption portOption({QStringLiteral("p"), QStringLiteral("port")},
                                  QStringLiteral("TCP port to listen on."), QStringLiteral("port"),
                                  QStringLiteral("1234"));
    QCommandLineOption dbOption({QStringLiteral("d"), QStringLiteral("db")},
                                QStringLiteral("Path of the SQLite file."), QStringLiteral("file"),
                                QStringLiteral("server.db"));
    parser.addOptions({portOption, dbOption});
    parser.process(app);

    DataBaseHandler &db = DataBaseHandler::instance();
    if (!db.openDataBase(parser.value(dbOption))) {
        qCritical() << "Failed to open database:" << parser.value(dbOption);
        return 1;
    }
    if (!db.createTables()) {
        qCritical() << "Failed to create tables.";
        return 1;
    }
    seedDemoData(db); // no-op unless the database is fresh

    Server server(&db);
    const quint16 port = quint16(qMax(1, parser.value(portOption).toInt()));
    if (!server.listen(QHostAddress::Any, port)) {
        qCritical() << "Server failed to start on port" << port << ":"
                    << server.errorString();
        return 1;
    }

    qInfo() << "IUT-Food server listening on port" << port;
    qInfo() << "Database:" << parser.value(dbOption);

    return app.exec();
}
