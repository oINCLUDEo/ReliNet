#include "itransport.hpp"
#include <QUdpSocket>
#include <QHostAddress>
#include <QDebug>

namespace ReliNet {

class UdpTransport : public ITransport {
    Q_OBJECT
    
public:
    explicit UdpTransport(QObject* parent = nullptr);
    ~UdpTransport() override;
    
    // ITransport interface
    QString transportName() const override { return "UDP Socket"; }
    TransportCapabilities capabilities() const override;
    TransportType transportType() const override { return TransportType::UDP; }
    
    bool isConnected() const override;
    QString getConnectionInfo() const override;
    
public slots:
    void connectToHost(const QString& address, uint16_t port) override;
    void disconnect() override;
    void sendData(const QByteArray& data) override;
    
private slots:
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    
private:
    QUdpSocket* socket_;
    QHostAddress remote_address_;
    uint16_t remote_port_ = 0;
    bool connected_ = false;
};

UdpTransport::UdpTransport(QObject* parent) : ITransport(parent), socket_(new QUdpSocket(this)) {
    connect(socket_, &QUdpSocket::readyRead, this, &UdpTransport::onSocketReadyRead);
    connect(socket_, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &UdpTransport::onSocketError);
}

UdpTransport::~UdpTransport() {
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
    }
}

TransportCapabilities UdpTransport::capabilities() const {
    TransportCapabilities caps;
    caps.multi_homing = false;
    caps.message_boundaries = true; // UDP preserves message boundaries
    caps.mtu = 1472; // Typical UDP payload size to avoid fragmentation
    caps.estimated_latency_ms = 20; // Lower latency than TCP
    caps.encrypted = false;
    caps.reliable = false; // UDP is unreliable
    caps.ordered = false;  // UDP doesn't guarantee ordering
    return caps;
}

bool UdpTransport::isConnected() const {
    return connected_;
}

QString UdpTransport::getConnectionInfo() const {
    if (connected_) {
        return QString("UDP %1:%2 -> %3:%4")
               .arg(socket_->localAddress().toString())
               .arg(socket_->localPort())
               .arg(remote_address_.toString())
               .arg(remote_port_);
    }
    return "UDP Disconnected";
}

void UdpTransport::connectToHost(const QString& address, uint16_t port) {
    remote_address_ = QHostAddress(address);
    remote_port_ = port;
    
    if (!socket_->bind()) {
        emit errorOccurred("Failed to bind UDP socket", TransportError::ConnectionFailed);
        return;
    }
    
    connected_ = true;
    emit connected();
    emit connectionStateChanged(true);
}

void UdpTransport::disconnect() {
    if (connected_) {
        socket_->disconnectFromHost();
        connected_ = false;
        emit disconnected();
        emit connectionStateChanged(false);
    }
}

void UdpTransport::sendData(const QByteArray& data) {
    if (!connected_) {
        emit errorOccurred("Cannot send data: not connected", TransportError::ConnectionFailed);
        return;
    }
    
    qint64 written = socket_->writeDatagram(data, remote_address_, remote_port_);
    if (written != data.size()) {
        emit errorOccurred("Failed to send UDP datagram", TransportError::TransportSpecific);
    }
}

void UdpTransport::onSocketReadyRead() {
    while (socket_->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(socket_->pendingDatagramSize());
        
        QHostAddress sender;
        quint16 senderPort;
        
        qint64 received = socket_->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        if (received > 0) {
            // Only accept data from expected sender
            if (sender == remote_address_ && senderPort == remote_port_) {
                datagram.resize(received);
                emit dataReceived(datagram);
            }
        }
    }
}

void UdpTransport::onSocketError(QAbstractSocket::SocketError error) {
    QString errorString;
    TransportError transportError = TransportError::TransportSpecific;
    
    switch (error) {
    case QAbstractSocket::HostNotFoundError:
        errorString = "Host not found";
        transportError = TransportError::NetworkUnreachable;
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

#include "udp_transport.moc"