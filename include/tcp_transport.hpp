#pragma once

#include "itransport.hpp"
#include <QTcpSocket>

namespace ReliNet {

class TcpTransport : public ITransport {
    Q_OBJECT

public:
    explicit TcpTransport(QObject* parent = nullptr);
    ~TcpTransport() override;

    QString transportName() const override { return "TCP Socket"; }
    TransportCapabilities capabilities() const override;
    TransportType transportType() const override { return TransportType::TCP; }

    bool isConnected() const override;
    QString getConnectionInfo() const override;

public slots:
    void connectToHost(const QString& address, uint16_t port) override;
    void disconnect() override;
    void sendData(const QByteArray& data) override;

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    QTcpSocket* socket_;
    QString remote_address_;
    uint16_t remote_port_ = 0;
    QByteArray read_buffer_;
};

} // namespace ReliNet
