#pragma once

#include "itransport.hpp"

namespace ReliNet {

// BluetoothTransport is a stub implementation.
// All methods return NotImplemented error.
// Full implementation deferred; interface is stable and ready for extension.
class BluetoothTransport : public ITransport {
    Q_OBJECT

public:
    explicit BluetoothTransport(QObject* parent = nullptr);
    ~BluetoothTransport() override;

    QString transportName() const override { return "Bluetooth (STUB)"; }
    TransportCapabilities capabilities() const override;
    TransportType transportType() const override { return TransportType::BLUETOOTH; }

    bool isConnected() const override { return false; }
    QString getConnectionInfo() const override { return "Bluetooth Not Implemented"; }

public slots:
    void connectToHost(const QString& address, uint16_t port) override;
    void disconnect() override;
    void sendData(const QByteArray& data) override;

private:
    void emitNotImplementedError(const QString& operation);
};

} // namespace ReliNet
