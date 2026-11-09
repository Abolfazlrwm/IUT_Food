//! Unit tests for the v2.3 TLS transport.  A real Server is started in
//! TLS mode with the bundled test certificate, then QSslSocket clients
//! perform the handshake and speak the JSON protocol over the encrypted
//! channel: login, wallet round-trip, a two-client chat broadcast and
//! the rejection of a plain-text client.  The suite skips itself when
//! the Qt build has no TLS backend (e.g. OpenSSL missing).
//!
//! The certificate/key pair lives in tests/certs/ (CN=localhost,
//! SAN 127.0.0.1) and is committed on purpose - it protects nothing.
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslCertificate>
#include <QSslCipher>
#include <QSslError>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "databasehandler.h"
#include "demodata.h"
#include "server.h"

#ifndef IUT_TEST_CERT_DIR
#define IUT_TEST_CERT_DIR "."
#endif

class TlsTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    Server *m_server = nullptr;
    quint16 m_port = 0;

    static QString certPath() { return QStringLiteral(IUT_TEST_CERT_DIR) + QStringLiteral("/test_cert.pem"); }
    static QString keyPath()  { return QStringLiteral(IUT_TEST_CERT_DIR) + QStringLiteral("/test_key.pem"); }

    //! Connects a QSslSocket to the test server and waits for the
    //! handshake.  The caller owns the socket (parented to the suite).
    //! Returns nullptr (after failing the test) when the handshake dies.
    QSslSocket *connectTlsClient()
    {
        auto *sock = new QSslSocket(this);
        // The self-signed test certificate must be accepted at signal
        // time: with Qt 6.8 a pre-connect ignoreSslErrors() alone does
        // not stop the handshake from aborting.
        QObject::connect(sock, &QSslSocket::sslErrors, sock,
                         [sock](const QList<QSslError> &) {
            sock->ignoreSslErrors();
        });
        sock->connectToHostEncrypted(QHostAddress(QHostAddress::LocalHost).toString(), m_port);

        QElapsedTimer timer;
        timer.start();
        while (!sock->isEncrypted() && sock->error() == QAbstractSocket::UnknownSocketError
               && timer.elapsed() < 5000)
            QTest::qWait(25);
        if (!sock->isEncrypted()) {
            QTest::qFail(qPrintable(QStringLiteral("TLS handshake failed: %1")
                                        .arg(sock->errorString())),
                         __FILE__, __LINE__);
            return nullptr;
        }
        return sock;
    }

    //! Sends one request line and collects the matching response over
    //! the encrypted channel; notifications that arrive meanwhile are
    //! returned through \p notifications.
    QJsonObject roundTrip(QSslSocket *sock, const QJsonObject &request,
                          QList<QJsonObject> *notifications = nullptr)
    {
        sock->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
        sock->flush();

        QElapsedTimer timer;
        timer.start();
        const QString wantCommand = request.value(QLatin1String("command")).toString();
        while (timer.elapsed() < 5000) {
            while (sock->canReadLine()) {
                const QJsonObject obj =
                    QJsonDocument::fromJson(sock->readLine()).object();
                if (obj.value(QLatin1String("type")).toString() == QLatin1String("response")
                    && obj.value(QLatin1String("command")).toString() == wantCommand)
                    return obj;
                if (notifications)
                    notifications->append(obj);
            }
            QTest::qWait(25);
        }
        return {};
    }

    //! Waits until a message of the given type arrives (for broadcasts).
    QList<QJsonObject> waitForType(QSslSocket *sock, const QString &type, int ms = 5000)
    {
        QList<QJsonObject> found;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < ms) {
            while (sock->canReadLine()) {
                const QJsonObject obj =
                    QJsonDocument::fromJson(sock->readLine()).object();
                if (obj.value(QLatin1String("type")).toString() == type)
                    found.append(obj);
            }
            if (!found.isEmpty())
                break;
            QTest::qWait(25);
        }
        return found;
    }

private slots:
    void initTestCase()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("No TLS backend available in this Qt build - skipping the TLS suite");

        QVERIFY(m_dir.isValid());
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.openDataBase(m_dir.filePath(QStringLiteral("tls_test.db"))));
        QVERIFY(db.createTables());
        seedDemoData(db); // demo_customer / 123456

        QVERIFY2(QSslCertificate::fromPath(certPath()).size() == 1,
                 "bundled test certificate must be loadable");

        m_server = new Server(&db, this);
        QVERIFY(m_server->enableTls(certPath(), keyPath()));
        QVERIFY(m_server->listen(QHostAddress::LocalHost, 0)); // ephemeral port
        m_port = m_server->serverPort();
        QVERIFY(m_port > 0);
    }

    void cleanupTestCase()
    {
        if (m_server)
            m_server->close();
    }

    //! The peer presents the bundled certificate and the channel really
    //! is encrypted afterwards.
    void handshakePresentsTheBundledCertificate()
    {
        std::unique_ptr<QSslSocket> sock(connectTlsClient());
        QVERIFY(sock);
        const QSslCertificate peer = sock->peerCertificate();
        QVERIFY(!peer.isNull());
        QCOMPARE(peer.subjectInfo(QSslCertificate::CommonName),
                 QStringList{QStringLiteral("localhost")});
        QVERIFY(sock->sessionCipher().usedBits() > 0);
    }

    //! Login and a wallet request travel encrypted end-to-end.
    void loginAndWalletWorkOverTls()
    {
        std::unique_ptr<QSslSocket> sock(connectTlsClient());
        QVERIFY(sock);

        const QJsonObject login = roundTrip(sock.get(), {
            {QStringLiteral("command"), QStringLiteral("login")},
            {QStringLiteral("username"), QStringLiteral("demo_customer")},
            {QStringLiteral("password"), QStringLiteral("123456")}});
        QVERIFY(login.value(QLatin1String("success")).toBool());
        QCOMPARE(login.value(QLatin1String("role")).toString(),
                 QStringLiteral("customer"));
        QCOMPARE(login.value(QLatin1String("session_token")).toString().size(), 64);

        // Session + wallet round-trip on the same encrypted channel.  The
        // balance reply carries no "success" field - the balance itself
        // is the answer (see PROTOCOL.md §7).
        const QJsonObject wallet = roundTrip(sock.get(), {
            {QStringLiteral("command"), QStringLiteral("get_wallet_balance")},
            {QStringLiteral("user_id"), login.value(QLatin1String("user_id")).toInt()},
            {QStringLiteral("token"),
             login.value(QLatin1String("session_token")).toString()}});
        QVERIFY(wallet.value(QLatin1String("balance")).toDouble() >= 0);
    }

    //! A plain-text client is not answered: the TLS listener aborts the
    //! connection instead of echoing protocol data in the clear.
    void plainTextClientIsRejected()
    {
        auto *sock = new QTcpSocket(this);
        sock->connectToHost(QHostAddress(QHostAddress::LocalHost), m_port);
        QVERIFY(sock->waitForConnected(3000));
        sock->write(QByteArrayLiteral("{\"command\":\"get_restaurants\"}\n"));
        sock->flush();

        QByteArray received;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 5000
               && sock->state() == QAbstractSocket::ConnectedState)
            QTest::qWait(25);
        while (sock->canReadLine())
            received += sock->readLine();

        const QJsonDocument doc = QJsonDocument::fromJson(received.trimmed());
        QVERIFY2(!doc.isObject(),
                 "the TLS server must not answer a plain-text request");
        QVERIFY2(sock->state() != QAbstractSocket::ConnectedState,
                 "the TLS server should close the failed handshake");
        sock->deleteLater();
    }

    //! Two encrypted clients exchange a chat broadcast through the server.
    void chatBroadcastReachesTheOtherTlsClient()
    {
        std::unique_ptr<QSslSocket> sender(connectTlsClient());
        std::unique_ptr<QSslSocket> listener(connectTlsClient());
        QVERIFY(sender && listener);

        const QJsonObject login = roundTrip(sender.get(), {
            {QStringLiteral("command"), QStringLiteral("login")},
            {QStringLiteral("username"), QStringLiteral("demo_customer")},
            {QStringLiteral("password"), QStringLiteral("123456")}});
        QVERIFY(login.value(QLatin1String("success")).toBool());
        const QJsonObject login2 = roundTrip(listener.get(), {
            {QStringLiteral("command"), QStringLiteral("login")},
            {QStringLiteral("username"), QStringLiteral("tls_diner")},
            {QStringLiteral("password"), QStringLiteral("pass1234")}});
        QVERIFY(login2.value(QLatin1String("success")).toBool());

        // send_chat_message has no direct reply - it only broadcasts.
        // Like every authenticated command it must carry the token that
        // came back from the login above.
        sender->write(QJsonDocument(QJsonObject{
            {QStringLiteral("command"), QStringLiteral("send_chat_message")},
            {QStringLiteral("order_id"), 1},
            {QStringLiteral("sender"), QStringLiteral("demo_customer")},
            {QStringLiteral("message_text"), QStringLiteral("TLS test message")},
            {QStringLiteral("token"),
             login.value(QLatin1String("session_token")).toString()}}
                              ).toJson(QJsonDocument::Compact) + '\n');
        sender->flush();

        const QList<QJsonObject> pushed =
            waitForType(listener.get(), QStringLiteral("new_chat_message_notification"));
        QVERIFY2(!pushed.isEmpty(), "chat broadcast must arrive over TLS");
        QCOMPARE(pushed.first().value(QLatin1String("data")).toObject()
                     .value(QLatin1String("message_text")).toString(),
                 QStringLiteral("TLS test message"));
    }

    void init()
    {
        // A second customer for the broadcast test; harmless elsewhere.
        auto &db = DataBaseHandler::instance();
        db.registerUser(QStringLiteral("tls_diner"), QStringLiteral("pass1234"),
                        QStringLiteral("customer"));
    }
};

QTEST_MAIN(TlsTests)
#include "tls_tests.moc"
