#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace ReliNet {

enum class TransportType {
    TCP,
    SCTP,
    UDP,
    SERIAL,
    SATELLITE,
    BLUETOOTH
};

struct TransportCapabilities {
    bool multi_homing = false;         // SCTP multi-homing support
    bool message_boundaries = false;   // UDP preserves message boundaries
    int mtu = 1500;                    // Maximum transmission unit
    int estimated_latency_ms = 100;    // Estimated one-way latency
    bool encrypted = false;            // Built-in encryption
    bool reliable = true;              // Reliable delivery guarantee
    bool ordered = true;               // Message ordering guarantee
};

enum class TransportError {
    NotImplemented,
    ConnectionFailed,
    NetworkUnreachable,
    Timeout,
    InvalidAddress,
    AuthenticationFailed,
    BufferFull,
    TransportSpecific
};

class ITransport : public QObject {
    Q_OBJECT
    
public:
    explicit ITransport(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ITransport() = default;
    
    // Pure virtual methods that must be implemented
    virtual QString transportName() const = 0;
    virtual TransportCapabilities capabilities() const = 0;
    virtual TransportType transportType() const = 0;
    
    // Connection state
    virtual bool isConnected() const = 0;
    virtual QString getConnectionInfo() const = 0;
    
    // Multi-homing support (for SCTP)
    virtual void setMultipleAddresses(const QStringList& local_addresses, 
                                     const QStringList& remote_addresses) {
        Q_UNUSED(local_addresses)
        Q_UNUSED(remote_addresses)
        // Default implementation does nothing - override in SCTP
    }
    
signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray& data);
    void errorOccurred(const QString& error, TransportError errorType = TransportError::TransportSpecific);
    void connectionStateChanged(bool connected);
    
public slots:
    virtual void connectToHost(const QString& address, uint16_t port) = 0;
    virtual void disconnect() = 0;
    virtual void sendData(const QByteArray& data) = 0;
    
    // Optional: set priority/stream for SCTP
    virtual void setPriority(int priority) { Q_UNUSED(priority) }
    virtual void setStream(int stream_id) { Q_UNUSED(stream_id) }
    
protected:
    // Helper method for derived classes
    static QString transportTypeToString(TransportType type) {
        switch (type) {
        case TransportType::TCP: return "TCP";
        case TransportType::SCTP: return "SCTP";
        case TransportType::UDP: return "UDP";
        case TransportType::SERIAL: return "Serial";
        case TransportType::SATELLITE: return "Satellite";
        case TransportType::BLUETOOTH: return "Bluetooth";
        default: return "Unknown";
        }
    }
};

} // namespace ReliNet