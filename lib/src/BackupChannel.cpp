#include "BackupChannel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace ReliNet {

BackupChannel::BackupChannel(const QString& host, quint16 port, QObject* parent)
    : IChannel(parent)
    , m_host(host)
    , m_port(port)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &BackupChannel::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &BackupChannel::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &BackupChannel::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &BackupChannel::onError);
}

BackupChannel::~BackupChannel()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void BackupChannel::send(const Message& msg)
{
    if (!isAvailable()) {
        emit sendFailed(msg.id(), "Channel not available");
        return;
    }

    QByteArray data = msg.serialize();
    data.append("\n");

    qint64 written = m_socket->write(data);
    if (written == -1) {
        emit sendFailed(msg.id(), "Failed to write to socket");
    } else {
        m_socket->flush();
        emit messageSent(msg.id());
    }
}

bool BackupChannel::isAvailable() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void BackupChannel::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "BackupChannel: Connecting to" << m_host << ":" << m_port;
        m_socket->connectToHost(m_host, m_port);
    }
}

void BackupChannel::disconnectFromServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void BackupChannel::onConnected()
{
    qDebug() << "BackupChannel: Connected to server";
    emit connected();
}

void BackupChannel::onDisconnected()
{
    qDebug() << "BackupChannel: Disconnected from server";
    emit disconnected();
}

void BackupChannel::onReadyRead()
{
    m_receiveBuffer.append(m_socket->readAll());

    int newlineIndex;
    while ((newlineIndex = m_receiveBuffer.indexOf('\n')) != -1) {
        QByteArray messageData = m_receiveBuffer.left(newlineIndex);
        m_receiveBuffer.remove(0, newlineIndex + 1);

        QJsonDocument doc = QJsonDocument::fromJson(messageData);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj["status"].toString() == "ACK") {
                QString messageId = obj["message_id"].toString();
                qDebug() << "BackupChannel: Received ACK for message" << messageId;
                emit ackReceived(messageId);
            }
        }
    }
}

void BackupChannel::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    qDebug() << "BackupChannel: Socket error:" << m_socket->errorString();
}

} // namespace ReliNet
