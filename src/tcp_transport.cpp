#include "itransport.hpp"
#include <QTcpSocket>
#include <QHostAddress>
#include <QDebug>

namespace ReliNet {

class TcpTransport : public ITransport {
    Q_OBJECT
    
public:
    explicit TcpTransport(QObject* parent = nullptr);
    ~TcpTransport() override;
    
    // ITransport interface
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

TcpTransport::TcpTransport(QObject* parent) : ITransport(parent), socket_(new QTcpSocket(this)) {
    connect(socket_, &QTcpSocket::connected, this, &TcpTransport::onSocketConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &TcpTransport::onSocketDisconnected);
    connect(socket_, &QTcpSocket::readyRead, this, &TcpTransport::onSocketReadyRead);
    connect(socket_, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &TcpTransport::onSocketError);
}

TcpTransport::~TcpTransport() {
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
    }
}

TransportCapabilities TcpTransport::capabilities() const {
    TransportCapabilities caps;
    caps.multi_homing = false;
    caps.message_boundaries = false;
    caps.mtu = 65535; // TCP can handle large segments
    caps.estimated_latency_ms = 50;
    caps.encrypted = false;
    caps.reliable = true;
    caps.ordered = true;
    return caps;
}

bool TcpTransport::isConnected() const {
    return socket_->state() == QAbstractSocket::ConnectedState;
}

QString TcpTransport::getConnectionInfo() const {
    if (isConnected()) {
        return QString("TCP %1:%2 -> %3:%4")
               .arg(socket_->localAddress().toString())
               .arg(socket_->localPort())
               .arg(socket_->peerAddress().toString())
               .arg(socket_->peerPort());
    }
    return "TCP Disconnected";
}

void TcpTransport::connectToHost(const QString& address, uint16_t port) {
    remote_address_ = address;
    remote_port_ = port;
    
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
    }
    
    socket_->connectToHost(address, port);
}

void TcpTransport::disconnect() {
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
    }
}

void TcpTransport::sendData(const QByteArray& data) {
    if (!isConnected()) {
        emit errorOccurred("Cannot send data: not connected", TransportError::ConnectionFailed);
        return;
    }
    
    qint64 written = socket_->write(data);
    if (written != data.size()) {
        emit errorOccurred("Failed to send all data", TransportError::TransportSpecific);
    }
}

void TcpTransport::onSocketConnected() {
    emit connected();
    emit connectionStateChanged(true);
}

void TcpTransport::onSocketDisconnected() {
    emit disconnected();
    emit connectionStateChanged(false);
}

void TcpTransport::onSocketReadyRead() {
    QByteArray data = socket_->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void TcpTransport::onSocketError(QAbstractSocket::SocketError error) {
    QString errorString;
    TransportError transportError = TransportError::TransportSpecific;
    
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        errorString = "Connection refused";
        transportError = TransportError::ConnectionFailed;
        break;
    case QAbstractSocket::HostNotFoundError:
        errorString = "Host not found";
        transportError = TransportError::NetworkUnreachable;
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorString = "Connection timeout";
        transportError = TransportError::Timeout;
        break;
    case QAbstractSocket::NetworkError:
        errorString = "Network error";
        transportError = TransportError::NetworkUnreachable;
        break;
    default:
        errorString = socket_->errorString();
        break;
    }
    
    emit errorOccurred(errorString, transportError);
}

} // namespace ReliNet

#include "tcp_transport.moc"