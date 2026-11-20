#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDeadlineTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>
#include <QDebug>
#include <atomic>
#include <csignal>
#include <cstdio>

#include "databasehandler.h"
#include "demodata.h"
#include "auditlog.h"
#include "backupmanager.h"
#include "logger.h"
#include "metricsserver.h"
#include "server.h"
#include "version.h"

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

//! v2.7: the self-probe behind --healthcheck.  Connects like a mini
//! client, sends the unauthenticated ping command and waits for a pong
//! that names the server release.  Deliberately runs BEFORE the data-
//! base is opened: the probe binary is the same executable, but a
//! health check must never touch (or lock) the SQLite file of the
//! process it is probing - and inside the container the probe runs
//! while the real server already owns the database.
bool runHealthCheck(const QString &host, quint16 port, int timeoutMs = 5000)
{
    QTcpSocket socket;
    socket.connectToHost(host, port);
    if (!socket.waitForConnected(timeoutMs)) {
        std::fputs("unhealthy: cannot connect\n", stderr);
        return false;
    }
    socket.write(QByteArrayLiteral("{\"command\":\"ping\"}\n"));
    socket.flush();
    const QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired()) {
        if (!socket.waitForReadyRead(qMax(1, int(deadline.remainingTime()))))
            break;
        while (socket.canReadLine()) {
            const QJsonObject obj =
                QJsonDocument::fromJson(socket.readLine().trimmed()).object();
            if (obj.value(QLatin1String("type")).toString() == QLatin1String("pong")) {
                std::printf("healthy: server version %s\n",
                            qPrintable(obj.value(QLatin1String("server_version"))
                                           .toString()));
                return true;
            }
        }
    }
    std::fputs("unhealthy: no pong within the timeout\n", stderr);
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("IUT-Food Server"));
    // v2.7: this used to be a hand-synced QStringLiteral("2.5.0") that
    // drifted while the changelog claimed one version header - it now
    // reads the same macro as CMake and the client About dialog.
    QCoreApplication::setApplicationVersion(QString::fromLatin1(IUT_FOOD_VERSION));

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
    // v2.5: rolling SQLite snapshots + a machine-readable audit trail.
    QCommandLineOption backupDirOption(QStringLiteral("backup-dir"),
                                       QStringLiteral("Directory for rolling SQLite "
                                                      "snapshots (VACUUM INTO). Without "
                                                      "it, backups stay disabled."),
                                       QStringLiteral("dir"));
    QCommandLineOption backupIntervalOption(QStringLiteral("backup-interval"),
                                            QStringLiteral("Minutes between snapshots "
                                                           "(default 60)."),
                                            QStringLiteral("minutes"),
                                            QStringLiteral("60"));
    QCommandLineOption backupKeepOption(QStringLiteral("backup-keep"),
                                        QStringLiteral("How many snapshot generations to "
                                                       "keep (default 5)."),
                                        QStringLiteral("count"),
                                        QStringLiteral("5"));
    QCommandLineOption auditFileOption(QStringLiteral("audit-file"),
                                       QStringLiteral("Append one JSON object per security-"
                                                      "relevant event (logins, admin "
                                                      "moderation, wallet moves) to this "
                                                      "JSONL file (rotated at 5 MiB)."),
                                       QStringLiteral("file"));
    // v2.7: Prometheus scrape endpoint + the container health check.
    QCommandLineOption metricsPortOption(QStringLiteral("metrics-port"),
                                         QStringLiteral("Serve Prometheus metrics (GET "
                                                        "/metrics, plain HTTP) on this "
                                                        "port. Without it, metrics stay "
                                                        "disabled."),
                                         QStringLiteral("port"));
    QCommandLineOption healthCheckOption(QStringLiteral("healthcheck"),
                                         QStringLiteral("Probe a running server (send "
                                                        "ping, expect pong) and exit "
                                                        "0/1 instead of serving.  Used "
                                                        "by Docker HEALTHCHECK."));
    QCommandLineOption hostOption(QStringLiteral("host"),
                                  QStringLiteral("Host for --healthcheck (default "
                                                 "127.0.0.1)."),
                                  QStringLiteral("host"),
                                  QStringLiteral("127.0.0.1"));
    parser.addOptions({portOption, dbOption, tlsCertOption, tlsKeyOption,
                       logFileOption, logLevelOption, backupDirOption,
                       backupIntervalOption, backupKeepOption, auditFileOption,
                       metricsPortOption, healthCheckOption, hostOption});
    parser.process(app);

    // v2.7: the health check must run before any database is touched -
    // it probes a REMOTE server, and inside the container the real
    // server already owns the SQLite file.
    if (parser.isSet(healthCheckOption)) {
        const quint16 probePort = quint16(qMax(1, parser.value(portOption).toInt()));
        return runHealthCheck(parser.value(hostOption), probePort) ? 0 : 1;
    }

    Logger::instance().configure(parser.value(logFileOption),
                                 Logger::levelFromString(parser.value(logLevelOption)));
    AuditLog::instance().configure(parser.value(auditFileOption));

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
    } else {
        LOG_WARNING("server",
                    "TLS is OFF: passwords and session tokens cross the network "
                    "in cleartext. Generate a certificate with "
                    "scripts/generate_certs.sh and pass --tls-cert/--tls-key; "
                    "the desktop client connects with TLS by default.");
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
    if (parser.isSet(auditFileOption))
        LOG_INFO("server", QString("audit file: %1").arg(parser.value(auditFileOption)));

    // v2.7: optional Prometheus scrape endpoint.  Plain HTTP on purpose
    // (scraping runs on an internal network); see docs/METRICS.md.
    MetricsServer metrics(&server);
    if (parser.isSet(metricsPortOption)) {
        const quint16 metricsPort = quint16(qMax(1, parser.value(metricsPortOption).toInt()));
        if (!metrics.listen(QHostAddress::Any, metricsPort)) {
            LOG_ERROR("server", QString("failed to open the metrics port %1: %2")
                                    .arg(metricsPort).arg(metrics.errorString()));
            return 1;
        }
        LOG_INFO("server", QString("Prometheus metrics on http://0.0.0.0:%1/metrics")
                               .arg(metricsPort));
    }

    // v2.5: rolling backups.  The manager owns a QTimer on this event
    // loop; it takes a baseline snapshot right away and then one every
    // --backup-interval minutes, keeping --backup-keep generations.
    BackupManager backups(&app);
    if (parser.isSet(backupDirOption)) {
        backups.configure(dbPath,
                          parser.value(backupDirOption),
                          qMax(1, parser.value(backupIntervalOption).toInt()),
                          qMax(1, parser.value(backupKeepOption).toInt()));
        QObject::connect(&backups, &BackupManager::backupFinished,
                         [](bool ok, const QString &message) {
                             if (ok)
                                 LOG_INFO("backup", QString("snapshot written: %1").arg(message));
                             else
                                 LOG_WARNING("backup", QString("snapshot failed: %1").arg(message));
                         });
        backups.start();
    }

    // v2.4: graceful shutdown.  Ctrl+C / SIGTERM / docker stop flip an
    // atomic flag; this watcher performs the real work on the event
    // loop - stop accepting, say goodbye to the clients, flush, exit.
    installStopHandlers();
    QTimer stopWatcher;
    QObject::connect(&stopWatcher, &QTimer::timeout, [&app, &server, &stopWatcher, &backups] {
        if (!g_stopRequested.load())
            return;
        stopWatcher.stop();
        LOG_INFO("server", "shutdown signal received - closing listener");
        server.close();             // stop accepting new connections
        server.disconnectClients(); // polite goodbye to the connected ones
        QTimer::singleShot(150, &app, [&app, &backups] { // let the goodbye bytes flush
            // v2.5: one final snapshot so the backup trail ends on a
            // fresh copy even when the server is stopped early.  A
            // no-op (false) when backups were never configured.
            backups.backupNow();
            LOG_INFO("server", "shutdown complete");
            app.quit();
        });
    });
    stopWatcher.start(200);

    return app.exec();
}
