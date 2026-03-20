#include "PrimaryChannel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace ReliNet {

PrimaryChannel::PrimaryChannel(const QString& host, quint16 port, QObject* parent)
    : IChannel(parent)
    , m_host(host)
    , m_port(port)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &PrimaryChannel::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &PrimaryChannel::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &PrimaryChannel::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &PrimaryChannel::onError);
}

PrimaryChannel::~PrimaryChannel()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void PrimaryChannel::send(const Message& msg)
{
    if (!isAvailable()) {
        emit sendFailed(msg.id(), "Channel not available");
        return;
    }

    QByteArray data = msg.serialize();
    // Add message delimiter
    data.append("\n");

    qint64 written = m_socket->write(data);
    if (written == -1) {
        emit sendFailed(msg.id(), "Failed to write to socket");
    } else {
        m_socket->flush();
        emit messageSent(msg.id());
    }
}

bool PrimaryChannel::isAvailable() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void PrimaryChannel::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "PrimaryChannel: Connecting to" << m_host << ":" << m_port;
        m_socket->connectToHost(m_host, m_port);
    }
}

void PrimaryChannel::disconnectFromServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void PrimaryChannel::onConnected()
{
    qDebug() << "PrimaryChannel: Connected to server";
    emit connected();
}

void PrimaryChannel::onDisconnected()
{
    qDebug() << "PrimaryChannel: Disconnected from server";
    emit disconnected();
}

void PrimaryChannel::onReadyRead()
{
    m_receiveBuffer.append(m_socket->readAll());

    // Process complete messages (delimited by newline)
    int newlineIndex;
    while ((newlineIndex = m_receiveBuffer.indexOf('\n')) != -1) {
        QByteArray messageData = m_receiveBuffer.left(newlineIndex);
        m_receiveBuffer.remove(0, newlineIndex + 1);

        // Parse JSON
        QJsonDocument doc = QJsonDocument::fromJson(messageData);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj["status"].toString() == "ACK") {
                QString messageId = obj["message_id"].toString();
                qDebug() << "PrimaryChannel: Received ACK for message" << messageId;
                emit ackReceived(messageId);
            }
        }
    }
}

void PrimaryChannel::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    qDebug() << "PrimaryChannel: Socket error:" << m_socket->errorString();
}

} // namespace ReliNet
