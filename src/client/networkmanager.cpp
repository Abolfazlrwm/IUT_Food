#include "networkmanager.h"

#include <QJsonDocument>
#include <QSslError>
#include <QSslSocket>

NetworkManager &NetworkManager::instance()
{
    static NetworkManager instance;
    return instance;
}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
    ensureSocketType();
}

void NetworkManager::ensureSocketType()
{
    const bool socketIsTls = qobject_cast<QSslSocket *>(m_socket) != nullptr;
    if (socketIsTls == m_useTls && m_socket)
        return;

    if (m_socket)
        m_socket->deleteLater();
    m_socket = m_useTls ? new QSslSocket(this) : new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, [this] {
        emit connected();
        // A login requested before the handshake finished goes out now.
        // (For QSslSocket the bytes are buffered until encryption starts.)
        if (!m_pendingUser.isEmpty()) {
            sendJson(QJsonObject{
                {QStringLiteral("command"), QStringLiteral("login")},
                {QStringLiteral("username"), m_pendingUser},
                {QStringLiteral("password"), m_pendingPassword}});
            m_pendingUser.clear();
            m_pendingPassword.clear();
        }
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        // The session died with the connection; the next authenticateWith
        // call (e.g. after a re-login) opens a fresh one.
        m_sessionToken.clear();
        m_pendingBeforeSession.clear();
        m_pendingUser.clear();
        m_pendingPassword.clear();
        emit disconnected();
    });
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit connectionError(m_socket->errorString());
    });

    if (auto *ssl = qobject_cast<QSslSocket *>(m_socket)) {
        // Self-signed demo certificates (scripts/generate_certs.sh) fail
        // the default CA check; the login dialog decides whether they are
        // acceptable.  Without that consent the handshake aborts and the
        // user sees the certificate error.
        connect(ssl, &QSslSocket::sslErrors, this,
                [this, ssl](const QList<QSslError> &errors) {
            if (m_acceptSelfSigned) {
                ssl->ignoreSslErrors();
                return;
            }
            emit connectionError(errors.isEmpty()
                                     ? QStringLiteral("TLS certificate rejected")
                                     : errors.first().errorString());
        });
    }
}

void NetworkManager::connectToServer(const QString &host, quint16 port,
                                     bool useTls, bool acceptSelfSigned)
{
    m_host = host;
    m_port = port;
    m_useTls = useTls;
    m_acceptSelfSigned = acceptSelfSigned;
    connectNow();
}

void NetworkManager::connectNow()
{
    ensureSocketType();
    if (m_socket->state() == QAbstractSocket::ConnectedState
        || m_socket->state() == QAbstractSocket::ConnectingState)
        return;
    m_sessionToken.clear();
    if (auto *ssl = qobject_cast<QSslSocket *>(m_socket)) {
        if (m_acceptSelfSigned)
            ssl->ignoreSslErrors(); // apply to the upcoming handshake
        ssl->connectToHostEncrypted(m_host, m_port);
    } else {
        m_socket->connectToHost(m_host, m_port);
    }
}

void NetworkManager::disconnectFromServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->disconnectFromHost();
}

bool NetworkManager::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool NetworkManager::isUsingTls() const
{
    return qobject_cast<QSslSocket *>(m_socket) != nullptr;
}

void NetworkManager::authenticateWith(const QString &username, const QString &password)
{
    // Called right after the local login succeeded.  The server's
    // `login` reply carries the session token used from here on.  When
    // the TCP/TLS handshake is still in flight the credentials are
    // parked and the login goes out from the connected() handler above.
    m_sessionToken.clear();
    m_pendingBeforeSession.clear();
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        sendJson(QJsonObject{
            {QStringLiteral("command"), QStringLiteral("login")},
            {QStringLiteral("username"), username},
            {QStringLiteral("password"), password}});
    } else {
        m_pendingUser = username;
        m_pendingPassword = password;
        connectNow(); // no-op when a connection attempt is already running
    }
}

void NetworkManager::dispatch(const QJsonObject &jsonObj)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    QJsonObject out = jsonObj;
    // v2.2 protocol: every authenticated command carries the token.
    if (!m_sessionToken.isEmpty())
        out[QStringLiteral("token")] = m_sessionToken;
    const QByteArray payload =
        QJsonDocument(out).toJson(QJsonDocument::Compact) + '\n';
    m_socket->write(payload);
    m_socket->flush();
}

void NetworkManager::flushQueue()
{
    while (!m_pendingBeforeSession.isEmpty())
        dispatch(m_pendingBeforeSession.dequeue());
}

void NetworkManager::sendJson(const QJsonObject &jsonObj)
{
    const QString command = jsonObj.value(QLatin1String("command")).toString();

    // The login request itself never needs a token.
    if (command == QLatin1String("login")) {
        dispatch(jsonObj);
        return;
    }

    // Panels fire their first queries straight from constructors, often
    // before the login round-trip has finished - park them until the
    // session is ready instead of letting the server reject them.
    if (!m_sessionToken.isEmpty()) {
        dispatch(jsonObj);
        return;
    }
    if (m_pendingBeforeSession.size() >= MaxPendingBeforeSession)
        m_pendingBeforeSession.dequeue(); // drop oldest under pressure
    m_pendingBeforeSession.enqueue(jsonObj);
}

void NetworkManager::onReadyRead()
{
    // The server replies are '\n'-terminated; buffer partial reads.
    while (m_socket->canReadLine()) {
        const QByteArray line = m_socket->readLine().trimmed();
        if (line.isEmpty())
            continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject())
            continue;

        const QJsonObject obj = doc.object();
        emit jsonReceived(obj);

        const QString type = obj.value(QLatin1String("type")).toString();
        const QString command = obj.value(QLatin1String("command")).toString();
        if (command == QLatin1String("login") && type == QLatin1String("response")) {
            if (obj.value(QLatin1String("success")).toBool()) {
                m_sessionToken = obj.value(QLatin1String("session_token")).toString();
                flushQueue(); // release the requests parked before the session
            }
            emit sessionEstablished(obj.value(QLatin1String("success")).toBool(),
                                    obj.value(QLatin1String("role")).toString(),
                                    obj.value(QLatin1String("user_id")).toInt());
        } else if (type == QLatin1String("restaurant_list_response")) {
            emit restaurantsReceived(obj.value(QLatin1String("data")).toArray());
        } else if (type == QLatin1String("order_status_update_notification")) {
            emit orderStatusUpdated(obj.value(QLatin1String("data")).toObject());
        } else if (type == QLatin1String("new_chat_message_notification")) {
            emit chatMessageReceived(obj.value(QLatin1String("data")).toObject());
        } else if (type == QLatin1String("response")) {
            emit operationResult(command,
                                 obj.value(QLatin1String("success")).toBool(),
                                 obj.value(QLatin1String("message")).toString());
        }
    }
}
