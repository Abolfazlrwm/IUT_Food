#include <QCoreApplication>
#include <QCommandLineParser>
#include <QHostAddress>
#include <QTimer>
#include <QDebug>
#include <atomic>
#include <csignal>

#include "databasehandler.h"
#include "demodata.h"
#include "logger.h"
#include "server.h"

namespace {

std::atomic<bool> g_stopRequested{false};

//! POSIX signal path: only an async-signal-safe atomic store happens
//! here; the event loop polls the flag and performs the real shutdown.
void requestStop(int)
{
    g_stopRequested.store(true);
}

#ifdef _WIN32
#include <windows.h>

BOOL WINAPI consoleCtrlHandler(DWORD)
{
    g_stopRequested.store(true);
    return TRUE; // let the event loop do the shutdown work
}
void installStopHandlers()
{
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
}
#else
void installStopHandlers()
{
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    // A client vanishing mid-write must never kill the process.
    std::signal(SIGPIPE, SIG_IGN);
}
#endif

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("IUT-Food Server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.4.0"));

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
    // v2.3: pass both flags to wrap every connection in TLS.  Self-signed
    // certificates are fine for a demo - scripts/generate_certs.sh creates one.
    QCommandLineOption tlsCertOption(QStringLiteral("tls-cert"),
                                     QStringLiteral("PEM certificate chain; enables TLS mode."),
                                     QStringLiteral("file"));
    QCommandLineOption tlsKeyOption(QStringLiteral("tls-key"),
                                    QStringLiteral("PEM private key matching --tls-cert."),
                                    QStringLiteral("file"));
    // v2.4: persistent, size-rotated log file + minimum severity.
    QCommandLineOption logFileOption(QStringLiteral("log-file"),
                                     QStringLiteral("Append logs to this file (rotated at 5 MiB, "
                                                    "3 generations kept). Without it, logs go "
                                                    "to stderr only."),
                                     QStringLiteral("file"));
    QCommandLineOption logLevelOption(QStringLiteral("log-level"),
                                      QStringLiteral("Minimum severity: debug, info, warning "
                                                     "or error."),
                                      QStringLiteral("level"),
                                      QStringLiteral("info"));
    parser.addOptions({portOption, dbOption, tlsCertOption, tlsKeyOption,
                       logFileOption, logLevelOption});
    parser.process(app);

    Logger::instance().configure(parser.value(logFileOption),
                                 Logger::levelFromString(parser.value(logLevelOption)));

    DataBaseHandler &db = DataBaseHandler::instance();
    const QString dbPath = parser.isSet(dbOption)
        ? parser.value(dbOption)
        : QCoreApplication::applicationDirPath() + QStringLiteral("/iut_food.db");
    if (!db.openDataBase(dbPath)) {
        LOG_ERROR("server", QString("failed to open database: %1").arg(dbPath));
        return 1;
    }
    if (!db.createTables()) {
        LOG_ERROR("server", "failed to create tables");
        return 1;
    }
    seedDemoData(db); // no-op unless the database is fresh

    Server server(&db);
    const bool tlsRequested = parser.isSet(tlsCertOption) || parser.isSet(tlsKeyOption);
    if (tlsRequested) {
        if (!parser.isSet(tlsCertOption) || !parser.isSet(tlsKeyOption)) {
            LOG_ERROR("server", "--tls-cert and --tls-key must be used together.");
            return 1;
        }
        if (!server.enableTls(parser.value(tlsCertOption), parser.value(tlsKeyOption))) {
            LOG_ERROR("server", "failed to load the TLS certificate and key - aborting.");
            return 1;
        }
    }
    const quint16 port = quint16(qMax(1, parser.value(portOption).toInt()));
    if (!server.listen(QHostAddress::Any, port)) {
        LOG_ERROR("server", QString("failed to start on port %1: %2")
                                .arg(port).arg(server.errorString()));
        return 1;
    }

    LOG_INFO("server", QString("listening on port %1 %2 | database: %3")
                           .arg(port)
                           .arg(server.isTlsEnabled() ? QStringLiteral("(TLS enabled)")
                                                      : QStringLiteral("(plain TCP)"),
                               dbPath));
    if (parser.isSet(logFileOption))
        LOG_INFO("server", QString("log file: %1").arg(parser.value(logFileOption)));

    // v2.4: graceful shutdown.  Ctrl+C / SIGTERM / docker stop flip an
    // atomic flag; this watcher performs the real work on the event
    // loop - stop accepting, say goodbye to the clients, flush, exit.
    installStopHandlers();
    QTimer stopWatcher;
    QObject::connect(&stopWatcher, &QTimer::timeout, [&app, &server, &stopWatcher] {
        if (!g_stopRequested.load())
            return;
        stopWatcher.stop();
        LOG_INFO("server", "shutdown signal received - closing listener");
        server.close();             // stop accepting new connections
        server.disconnectClients(); // polite goodbye to the connected ones
        QTimer::singleShot(150, &app, [&app] { // let the goodbye bytes flush
            LOG_INFO("server", "shutdown complete");
            app.quit();
        });
    });
    stopWatcher.start(200);

    return app.exec();
}
