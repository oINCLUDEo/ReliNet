#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QDebug>

class MessageServer : public QObject {
    Q_OBJECT

public:
    explicit MessageServer(quint16 port, QObject* parent = nullptr)
        : QObject(parent)
        , m_server(new QTcpServer(this))
        , m_port(port)
    {
        connect(m_server, &QTcpServer::newConnection, this, &MessageServer::onNewConnection);
    }

    bool start() {
        if (m_server->listen(QHostAddress::Any, m_port)) {
            qDebug() << "Server listening on port" << m_port;
            return true;
        } else {
            qDebug() << "Failed to start server:" << m_server->errorString();
            return false;
        }
    }

private slots:
    void onNewConnection() {
        QTcpSocket* socket = m_server->nextPendingConnection();
        qDebug() << "New client connected:" << socket->peerAddress().toString();

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            onReadyRead(socket);
        });

        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }

    void onReadyRead(QTcpSocket* socket) {
        QByteArray& buffer = m_clientBuffers[socket];
        buffer.append(socket->readAll());

        // Process complete messages (delimited by newline)
        int newlineIndex;
        while ((newlineIndex = buffer.indexOf('\n')) != -1) {
            QByteArray messageData = buffer.left(newlineIndex);
            buffer.remove(0, newlineIndex + 1);

            processMessage(socket, messageData);
        }
    }

    void processMessage(QTcpSocket* socket, const QByteArray& data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qDebug() << "Invalid JSON received";
            return;
        }

        QJsonObject obj = doc.object();
        QString messageId = obj["id"].toString();

        // Check for duplicate
        if (m_receivedMessages.contains(messageId)) {
            qDebug() << "Duplicate message received:" << messageId << "- sending ACK again";
        } else {
            m_receivedMessages.insert(messageId);
            qDebug() << "Message received:" << messageId
                     << "Priority:" << obj["priority"].toString()
                     << "Payload:" << obj["payload"].toString();
        }

        // Send ACK
        QJsonObject ack;
        ack["message_id"] = messageId;
        ack["status"] = "ACK";

        QJsonDocument ackDoc(ack);
        QByteArray ackData = ackDoc.toJson(QJsonDocument::Compact);
        ackData.append("\n");

        socket->write(ackData);
        socket->flush();

        qDebug() << "ACK sent for message:" << messageId;
    }

private:
    QTcpServer* m_server;
    quint16 m_port;
    QSet<QString> m_receivedMessages;
    QMap<QTcpSocket*, QByteArray> m_clientBuffers;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    quint16 primaryPort = 12345;
    quint16 backupPort = 12346;

    // Primary server
    MessageServer primaryServer(primaryPort);
    if (!primaryServer.start()) {
        return 1;
    }

    // Backup server
    MessageServer backupServer(backupPort);
    if (!backupServer.start()) {
        return 1;
    }

    qDebug() << "ReliNet Server started";
    qDebug() << "Primary port:" << primaryPort;
    qDebug() << "Backup port:" << backupPort;

    return app.exec();
}

#include "server_main.moc"
