#ifndef METRICSSERVER_H
#define METRICSSERVER_H

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QStringList>

#include "server.h"
#include "version.h"

/*!
 * v2.7: a tiny HTTP listener that renders the process counters in the
 * Prometheus text exposition format (version 0.0.4), so the server can
 * be scraped by Prometheus / OpenMetrics consumers without any admin
 * login or IUT-Food client.
 *
 *   iut-food-server --port 1234 --metrics-port 9101
 *   curl http://127.0.0.1:9101/metrics
 *
 * Design notes:
 * - The endpoint is read-only and reveals no business data - it exposes
 *   exactly the same counters the admin-only `get_server_stats` command
 *   returns, plus a build_info label carrying the release version.
 * - The metrics port is PLAIN HTTP by design: Prometheus scraping is
 *   meant for an internal network (or a sidecar/reverse proxy that
 *   terminates TLS).  Keep it firewalled from the internet.
 * - One-shot HTTP/1.0 semantics: every request gets one response, the
 *   socket closes and the connection is cleaned up.  No keep-alive.
 * - Header-only and split into renderMetrics() (pure, unit-testable)
 *   and the socket plumbing (thin), mirroring the project style.
 */
// Note: deliberately no Q_OBJECT.  The class declares no signals or
// slots of its own (the socket plumbing uses lambdas with a context
// object), and skipping it keeps the header-only class free of moc
// linkage requirements for every target that includes it.
class MetricsServer : public QTcpServer
{
public:
    explicit MetricsServer(Server *server, QObject *parent = nullptr)
        : QTcpServer(parent), m_server(server)
    {
    }

    //! True when \p path is served (anything else gets a 404, so a
    //! scraper pointing at the wrong URL says so instead of hanging).
    static bool isMetricsPath(const QString &path)
    {
        return path == QLatin1String("/metrics") || path == QLatin1String("/metrics/");
    }

    //! Escapes a Prometheus label value (" and \ plus newlines).
    static QString escapeLabel(QString value)
    {
        value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        value.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
        return value;
    }

    //! Renders the current counters as Prometheus text.  Split from the
    //! socket plumbing so the unit suite can verify the exact output
    //! without any network round trip.
    QString renderMetrics(qint64 nowMs = 0) const
    {
        const qint64 now = nowMs > 0 ? nowMs : QDateTime::currentMSecsSinceEpoch();
        const ServerStats *s = m_server ? &m_server->stats() : nullptr;
        const SessionManager *sessions = m_server ? &m_server->sessions() : nullptr;
        const auto counter = [s](const std::atomic<quint64> ServerStats::*field) -> qlonglong {
            if (!s)
                return 0;
            return qlonglong((s->*field).load(std::memory_order_relaxed));
        };

        QStringList lines;
        lines
            << QStringLiteral("# HELP iut_food_build_info Release metadata (always 1).")
            << QStringLiteral("# TYPE iut_food_build_info gauge")
            << QStringLiteral("iut_food_build_info{version=\"%1\"} 1")
                   .arg(escapeLabel(QString::fromLatin1(IUT_FOOD_VERSION)))
            << QStringLiteral("# HELP iut_food_uptime_seconds Seconds since the server started.")
            << QStringLiteral("# TYPE iut_food_uptime_seconds gauge")
            << QStringLiteral("iut_food_uptime_seconds %1")
                   .arg(s ? qlonglong((now - s->startedAtMs) / 1000) : qlonglong(0))
            << QStringLiteral("# HELP iut_food_sessions_active Session tokens currently valid.")
            << QStringLiteral("# TYPE iut_food_sessions_active gauge")
            << QStringLiteral("iut_food_sessions_active %1")
                   .arg(sessions ? qlonglong(sessions->activeCount(now)) : qlonglong(0))
            << QStringLiteral("# HELP iut_food_sessions_total Sessions opened since startup.")
            << QStringLiteral("# TYPE iut_food_sessions_total counter")
            << QStringLiteral("iut_food_sessions_total %1")
                   .arg(sessions ? qlonglong(sessions->totalOpened()) : qlonglong(0))
            << QStringLiteral("# HELP iut_food_connections_accepted_total TCP connections accepted.")
            << QStringLiteral("# TYPE iut_food_connections_accepted_total counter")
            << QStringLiteral("iut_food_connections_accepted_total %1")
                   .arg(counter(&ServerStats::connectionsAccepted))
            << QStringLiteral("# HELP iut_food_tls_connections_total Connections that completed a TLS handshake.")
            << QStringLiteral("# TYPE iut_food_tls_connections_total counter")
            << QStringLiteral("iut_food_tls_connections_total %1")
                   .arg(counter(&ServerStats::tlsConnections))
            << QStringLiteral("# HELP iut_food_requests_served_total Protocol commands dispatched.")
            << QStringLiteral("# TYPE iut_food_requests_served_total counter")
            << QStringLiteral("iut_food_requests_served_total %1")
                   .arg(counter(&ServerStats::requestsServed))
            << QStringLiteral("# HELP iut_food_requests_rejected_total Rate-limit / frame-cap rejections.")
            << QStringLiteral("# TYPE iut_food_requests_rejected_total counter")
            << QStringLiteral("iut_food_requests_rejected_total %1")
                   .arg(counter(&ServerStats::requestsRejected))
            << QStringLiteral("# HELP iut_food_auth_failures_total Missing, expired or low-role tokens.")
            << QStringLiteral("# TYPE iut_food_auth_failures_total counter")
            << QStringLiteral("iut_food_auth_failures_total %1")
                   .arg(counter(&ServerStats::authFailures));
        return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
    }

protected:
    //! One HTTP request per connection: buffer until the header block
    //! is complete (or a bare "GET /metrics\n" line arrived), answer,
    //! close.  Unknown paths get a 404, unknown methods a 405.
    void incomingConnection(qintptr socketDescriptor) override
    {
        auto *socket = new QTcpSocket(this);
        if (!socket->setSocketDescriptor(socketDescriptor)) {
            delete socket;
            return;
        }
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);

        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            QByteArray &buffer = m_buffers[socket];
            buffer += socket->readAll();
            const int headerEnd = buffer.indexOf("\r\n\r\n");
            if (headerEnd < 0 && !buffer.endsWith('\n'))
                return; // keep waiting for the rest of the request head

            const QByteArray head = headerEnd >= 0 ? buffer.left(headerEnd) : buffer;
            m_buffers.remove(socket);

            const QString headText = QString::fromUtf8(head);
            const QString firstLine = headText
                                          .section(QLatin1Char('\n'), 0, 0)
                                          .simplified();
            const QStringList parts = firstLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            const QString method = parts.value(0).toUpper();
            const QString path = parts.value(1);

            if (method != QLatin1String("GET") && method != QLatin1String("HEAD")) {
                writeResponse(socket, 405, QStringLiteral("Method Not Allowed\n"));
                return;
            }
            if (!isMetricsPath(path)) {
                writeResponse(socket, 404, QStringLiteral("Not Found\n"),
                              method == QLatin1String("HEAD"));
                return;
            }
            writeResponse(socket, 200,
                          renderMetrics(QDateTime::currentMSecsSinceEpoch()),
                          method == QLatin1String("HEAD"));
        });
    }

private:
    static void writeResponse(QTcpSocket *socket, int status, const QString &body,
                              bool headOnly = false)
    {
        const QByteArray payload = body.toUtf8();
        const char *reason = status == 200 ? "OK"
                            : status == 404 ? "Not Found"
                            : status == 405 ? "Method Not Allowed"
                                            : "Internal Server Error";
        QByteArray head;
        head += QByteArray("HTTP/1.1 ") + QByteArray::number(status) + ' ' + reason + "\r\n";
        head += "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n";
        head += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
        head += "Connection: close\r\n\r\n";
        socket->write(head);
        if (!headOnly)
            socket->write(payload);
        socket->flush();
        socket->disconnectFromHost();
    }

    //! Per-socket request buffers, keyed by pointer; entries are dropped
    //! the moment a request completes or the socket disappears.
    QHash<QTcpSocket *, QByteArray> m_buffers;
    Server *m_server = nullptr;
};

#endif // METRICSSERVER_H
