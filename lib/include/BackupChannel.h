#ifndef BACKUPCHANNEL_H
#define BACKUPCHANNEL_H

#include "IChannel.h"
#include <QTcpSocket>

namespace ReliNet {

class BackupChannel : public IChannel {
    Q_OBJECT

public:
    explicit BackupChannel(const QString& host, quint16 port, QObject* parent = nullptr);
    ~BackupChannel() override;

    void send(const Message& msg) override;
    bool isAvailable() const override;
    QString name() const override { return "Backup (TCP)"; }

    void connectToServer();
    void disconnectFromServer();

signals:
    void connected();
    void disconnected();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);

private:
    QString m_host;
    quint16 m_port;
    QTcpSocket* m_socket;
    QByteArray m_receiveBuffer;
};

} // namespace ReliNet

#endif // BACKUPCHANNEL_H
