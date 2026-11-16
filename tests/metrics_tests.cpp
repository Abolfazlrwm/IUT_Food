//! Unit tests for the v2.7 Prometheus metrics endpoint.  The rendering
//! half is verified as pure text (HELP/TYPE pairing, build info, live
//! counters); the HTTP half speaks real TCP against a MetricsServer on
//! an ephemeral port: 200 + Prometheus content type on /metrics, 404
//! elsewhere, 405 for non-GET methods.
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#include "databasehandler.h"
#include "demodata.h"
#include "metricsserver.h"
#include "server.h"
#include "version.h"

class MetricsTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    Server *m_server = nullptr;
    MetricsServer *m_metrics = nullptr;
    quint16 m_port = 0;

    //! One plain-HTTP request against the metrics listener; returns the
    //! full response bytes (head + body).  The wait loop uses
    //! QTest::qWait() on purpose: only a real processEvents() dispatch
    //! runs the QTcpServer accept notifier, so the server-side socket
    //! exists to answer.  waitForReadyRead() on the client socket does
    //! NOT do that (it only watches the client's own notifiers) - the
    //! request would sit unanswered forever.
    QByteArray httpGet(const QByteArray &request, int ms = 4000)
    {
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, m_port);
        QElapsedTimer timer;
        timer.start();
        while (sock.state() != QAbstractSocket::ConnectedState
               && timer.elapsed() < ms)
            QTest::qWait(25);
        if (sock.state() != QAbstractSocket::ConnectedState) {
            QTest::qFail(qPrintable(QStringLiteral("metrics connect failed: %1")
                                        .arg(sock.errorString())),
                         __FILE__, __LINE__);
            return {};
        }
        sock.write(request);
        sock.flush();
        all_response.clear();
        timer.restart();
        while (timer.elapsed() < ms) {
            QTest::qWait(20);
            all_response += sock.readAll();
            const int headEnd = all_response.indexOf("\r\n\r\n");
            if (headEnd >= 0
                && all_response.size() >= headEnd + 4 + parseContentLength(all_response))
                break;
        }
        return all_response;
    }

    //! Keeps the response bytes alive across the qWait() dispatches.
    QByteArray all_response;

    static int parseContentLength(const QByteArray &response)
    {
        const int at = response.indexOf("Content-Length: ");
        if (at < 0)
            return 0;
        const QByteArray value = response.mid(at + 16,
                                              response.indexOf("\r\n", at) - at - 16);
        return value.toInt();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.openDataBase(m_dir.filePath(QStringLiteral("metrics_test.db"))));
        QVERIFY(db.createTables());
        seedDemoData(db);

        m_server = new Server(&db, this);
        QVERIFY(m_server->listen(QHostAddress::LocalHost, 0)); // ephemeral port

        m_metrics = new MetricsServer(m_server, this);
        QVERIFY(m_metrics->listen(QHostAddress::LocalHost, 0));
        m_port = m_metrics->serverPort();
        QVERIFY(m_port > 0);
    }

    void cleanupTestCase()
    {
        if (m_server)
            m_server->close();
    }

    //! The text exposition format: every metric line is preceded by its
    //! HELP and TYPE lines, and the build info names the release macro.
    void renderIsWellFormedPrometheusText()
    {
        const QString body = m_metrics->renderMetrics();
        const QStringList lines = body.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

        int helpCount = 0, typeCount = 0, sampleCount = 0;
        for (const QString &line : lines) {
            if (line.startsWith(QLatin1String("# HELP")))
                ++helpCount;
            else if (line.startsWith(QLatin1String("# TYPE")))
                ++typeCount;
            else if (!line.startsWith(QLatin1Char('#')))
                ++sampleCount;
        }
        QCOMPARE(helpCount, 9);
        QCOMPARE(typeCount, 9);
        QCOMPARE(sampleCount, helpCount); // each family reports one sample

        QVERIFY(body.contains(QStringLiteral(
            "iut_food_build_info{version=\"%1\"} 1").arg(IUT_FOOD_VERSION)));
        QVERIFY(body.contains(QStringLiteral("iut_food_uptime_seconds ")));
        QVERIFY(body.contains(QStringLiteral("iut_food_sessions_active ")));
        QVERIFY(body.contains(QStringLiteral("iut_food_sessions_total ")));
        QVERIFY(body.contains(QStringLiteral("iut_food_connections_accepted_total ")));
        QVERIFY(body.contains(QStringLiteral("iut_food_tls_connections_total ")));
        QVERIFY(body.contains(QStringLiteral("iut_food_requests_served_total ")));
        QVERIFY(body.contains(QStringLiteral("iut_food_requests_rejected_total ")));
        QVERIFY(body.contains(QStringLiteral("iut_food_auth_failures_total ")));
    }

    //! The gauges must be honest: an opened session shows up as active,
    //! and the uptime is measured in whole seconds from server start.
    void gaugesTrackReality()
    {
        const QString token = m_server->sessions().openSession(
            QStringLiteral("metrics_probe"), QStringLiteral("customer"), 42,
            QDateTime::currentMSecsSinceEpoch());
        QVERIFY(token.size() == 64);

        const QString body = m_metrics->renderMetrics();
        const int active = valueOf(body, QStringLiteral("iut_food_sessions_active"));
        QVERIFY2(active >= 1, qPrintable(QStringLiteral("active sessions: %1").arg(active)));
        const qlonglong uptime =
            valueOf(body, QStringLiteral("iut_food_uptime_seconds"));
        QVERIFY(uptime >= 0);

        m_server->sessions().revoke(token);
        QVERIFY(valueOf(m_metrics->renderMetrics(),
                        QStringLiteral("iut_food_sessions_active")) == active - 1);
    }

    //! GET /metrics answers 200 with the Prometheus content type and the
    //! rendered families as the body - what a real scraper consumes.
    void httpGetServesPrometheusBody()
    {
        const QByteArray response = httpGet(QByteArrayLiteral("GET /metrics HTTP/1.1\r\nHost: localhost\r\nAccept: text/plain\r\n\r\n"));
        QVERIFY(response.startsWith("HTTP/1.1 200 OK\r\n"));
        QVERIFY(response.contains("Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"));
        const int bodyAt = response.indexOf("\r\n\r\n") + 4;
        const QString body = QString::fromUtf8(response.mid(bodyAt));
        QVERIFY(body.contains(QStringLiteral("# TYPE iut_food_build_info gauge")));
    }

    //! Anything but /metrics is a 404, non-GET methods a 405 - so a
    //! misconfigured scraper complains instead of scraping the wrong
    //! endpoint silently.
    void wrongPathAndMethodAreRejected()
    {
        const QByteArray notFound =
            httpGet(QByteArrayLiteral("GET /health HTTP/1.1\r\nHost: x\r\n\r\n"));
        QVERIFY(notFound.startsWith("HTTP/1.1 404 Not Found\r\n"));

        const QByteArray post =
            httpGet(QByteArrayLiteral("POST /metrics HTTP/1.1\r\nHost: x\r\n\r\n"));
        QVERIFY(post.startsWith("HTTP/1.1 405 Method Not Allowed\r\n"));

        const QByteArray head =
            httpGet(QByteArrayLiteral("HEAD /metrics HTTP/1.1\r\nHost: x\r\n\r\n"));
        QVERIFY(head.startsWith("HTTP/1.1 200 OK\r\n"));
        // HEAD returns headers only: the body must be absent.
        const int headerEnd = head.indexOf("\r\n\r\n") + 4;
        QCOMPARE(head.size() - headerEnd, 0);
    }

private:
    //! Parses "<metric> <value>" out of the rendered body.
    static qlonglong valueOf(const QString &body, const QString &metric)
    {
        const QStringList lines = body.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (line.startsWith(metric + QLatin1Char(' '))) {
                bool ok = false;
                const qlonglong value = line.mid(metric.size() + 1).toLongLong(&ok);
                if (ok)
                    return value;
            }
        }
        return -1;
    }
};

#include "metrics_tests.moc"

QTEST_MAIN(MetricsTests)
