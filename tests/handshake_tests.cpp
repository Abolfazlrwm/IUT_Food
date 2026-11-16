//! Unit tests for the v2.7 protocol handshake additions: the
//! unauthenticated `ping` liveness command (which also backs the
//! --healthcheck probe and the Docker HEALTHCHECK) and the version
//! handshake carried by every login response.  A real Server runs
//! in-process on an ephemeral plain-TCP port, exactly like the TLS
//! suite - only without the encryption.
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#include "databasehandler.h"
#include "demodata.h"
#include "server.h"
#include "version.h"

class HandshakeTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    Server *m_server = nullptr;
    quint16 m_port = 0;

    //! Sends one request line and collects the matching reply.  Matched
    //! by the echoed "command" field, because the pong answer carries
    //! "type": "pong" rather than the usual "response".
    QJsonObject roundTrip(QTcpSocket *sock, const QJsonObject &request, int ms = 5000)
    {
        sock->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
        sock->flush();
        QElapsedTimer timer;
        timer.start();
        const QString wantCommand = request.value(QLatin1String("command")).toString();
        while (timer.elapsed() < ms) {
            while (sock->canReadLine()) {
                const QJsonObject obj =
                    QJsonDocument::fromJson(sock->readLine()).object();
                if (obj.value(QLatin1String("command")).toString() == wantCommand)
                    return obj;
            }
            QTest::qWait(25);
        }
        return {};
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.openDataBase(m_dir.filePath(QStringLiteral("handshake_test.db"))));
        QVERIFY(db.createTables());
        seedDemoData(db); // demo_customer / 123456, admin / admin123

        m_server = new Server(&db, this);
        QVERIFY(m_server->listen(QHostAddress::LocalHost, 0)); // ephemeral port
        m_port = m_server->serverPort();
        QVERIFY(m_port > 0);
    }

    void cleanupTestCase()
    {
        if (m_server)
            m_server->close();
    }

    //! ping needs no session and answers pong with the release version
    //! plus a server timestamp - the contract --healthcheck depends on.
    void pingAnswersPongWithVersion()
    {
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, m_port);
        QVERIFY(sock.waitForConnected(3000));
        const QJsonObject pong = roundTrip(&sock,
            QJsonObject{{QStringLiteral("command"), QStringLiteral("ping")}});

        QCOMPARE(pong.value(QLatin1String("type")).toString(),
                 QStringLiteral("pong"));
        QCOMPARE(pong.value(QLatin1String("server_version")).toString(),
                 QString::fromLatin1(IUT_FOOD_VERSION));
        QVERIFY(pong.value(QLatin1String("server_time")).toDouble() > 0);
        sock.disconnectFromHost();
    }

    //! The success path still works - handshake fields are additive.
    void loginCarriesServerVersionWithoutWarning()
    {
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, m_port);
        QVERIFY(sock.waitForConnected(3000));
        const QJsonObject login = roundTrip(&sock, QJsonObject{
            {QStringLiteral("command"), QStringLiteral("login")},
            {QStringLiteral("username"), QStringLiteral("demo_customer")},
            {QStringLiteral("password"), QStringLiteral("123456")}});

        QVERIFY(login.value(QLatin1String("success")).toBool());
        QCOMPARE(login.value(QLatin1String("server_version")).toString(),
                 QString::fromLatin1(IUT_FOOD_VERSION));
        QVERIFY(!login.contains(QLatin1String("warning")));
        QVERIFY(login.value(QLatin1String("session_token")).toString().size() == 64);
        sock.disconnectFromHost();
    }

    //! A client reporting a lower MAJOR version is told, in the wire
    //! response, that it is outdated - the server still logs it in (the
    //! client decides how loudly to warn its user).
    void outdatedClientGetsWarning()
    {
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, m_port);
        QVERIFY(sock.waitForConnected(3000));
        const QJsonObject login = roundTrip(&sock, QJsonObject{
            {QStringLiteral("command"), QStringLiteral("login")},
            {QStringLiteral("username"), QStringLiteral("demo_customer")},
            {QStringLiteral("password"), QStringLiteral("123456")},
            {QStringLiteral("client_version"), QStringLiteral("1.0.0")}});

        QVERIFY(login.value(QLatin1String("success")).toBool());
        QCOMPARE(login.value(QLatin1String("warning")).toString(),
                 QStringLiteral("outdated_client"));
        sock.disconnectFromHost();
    }

    //! Same-major and same-version clients are silent: only a major
    //! drift is worth a warning, minor drift is tolerated.
    void currentMajorClientStaysSilent()
    {
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, m_port);
        QVERIFY(sock.waitForConnected(3000));

        const QString serverVersion = QString::fromLatin1(IUT_FOOD_VERSION);
        const QString sameMajor = serverVersion.section(QLatin1Char('.'), 0, 0)
            + QStringLiteral(".0.0");

        const QJsonObject login = roundTrip(&sock, QJsonObject{
            {QStringLiteral("command"), QStringLiteral("login")},
            {QStringLiteral("username"), QStringLiteral("demo_customer")},
            {QStringLiteral("password"), QStringLiteral("123456")},
            {QStringLiteral("client_version"), sameMajor}});
        QVERIFY(login.value(QLatin1String("success")).toBool());
        QVERIFY(!login.contains(QLatin1String("warning")));
        sock.disconnectFromHost();
    }
};

#include "handshake_tests.moc"

QTEST_MAIN(HandshakeTests)
