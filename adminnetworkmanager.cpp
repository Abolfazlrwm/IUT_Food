#include "adminnetworkmanager.h"
#include <QJsonDocument>
#include <QDebug>

AdminNetworkManager::AdminNetworkManager(QObject* parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &AdminNetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &AdminNetworkManager::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &AdminNetworkManager::disconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, [this](QAbstractSocket::SocketError){
                emit errorOccurred(m_socket->errorString());
            });
}

void AdminNetworkManager::connectToServer(const QString& ip, quint16 port)
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_socket->connectToHost(ip, port);
    }
}

void AdminNetworkManager::sendJson(const QJsonObject& obj)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) return;
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
    m_socket->flush();
}

void AdminNetworkManager::onReadyRead()
{
    while (m_socket->canReadLine()) {
        QByteArray line = m_socket->readLine().trimmed();
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) {
            emit serverResponse(doc.object());
        }
    }
}
