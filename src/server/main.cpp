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
    // Default: the SAME iut_food.db the client uses next to the binary, so
    // a same-machine demo (server + client) sees one consistent database.
    // The --db flag still allows split deployments.
    QCommandLineOption dbOption({QStringLiteral("d"), QStringLiteral("db")},
                                QStringLiteral("Path of the SQLite file."), QStringLiteral("file"),
                                QString());
    parser.addOptions({portOption, dbOption});
    parser.process(app);

    DataBaseHandler &db = DataBaseHandler::instance();
    const QString dbPath = parser.isSet(dbOption)
        ? parser.value(dbOption)
        : QCoreApplication::applicationDirPath() + QStringLiteral("/iut_food.db");
    if (!db.openDataBase(dbPath)) {
        qCritical() << "Failed to open database:" << dbPath;
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
    qInfo() << "Database:" << dbPath;

    return app.exec();
}
