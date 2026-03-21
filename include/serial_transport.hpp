#pragma once

#include "itransport.hpp"
#include <QSerialPort>

namespace ReliNet {

class SerialTransport : public ITransport {
    Q_OBJECT

public:
    explicit SerialTransport(QObject* parent = nullptr);
    ~SerialTransport() override;

    QString transportName() const override { return "Serial Port"; }
    TransportCapabilities capabilities() const override;
    TransportType transportType() const override { return TransportType::SERIAL; }

    bool isConnected() const override;
    QString getConnectionInfo() const override;

    void setBaudRate(qint32 baudRate) { baud_rate_ = baudRate; }
    void setDataBits(QSerialPort::DataBits dataBits) { data_bits_ = dataBits; }
    void setParity(QSerialPort::Parity parity) { parity_ = parity; }
    void setStopBits(QSerialPort::StopBits stopBits) { stop_bits_ = stopBits; }
    void setFlowControl(QSerialPort::FlowControl flowControl) { flow_control_ = flowControl; }

    static QStringList availablePorts();

public slots:
    void connectToHost(const QString& portName, uint16_t baudRate = 9600) override;
    void disconnect() override;
    void sendData(const QByteArray& data) override;

private slots:
    void onSerialReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    QSerialPort* serial_port_;
    QString port_name_;

    qint32 baud_rate_ = 9600;
    QSerialPort::DataBits data_bits_ = QSerialPort::Data8;
    QSerialPort::Parity parity_ = QSerialPort::NoParity;
    QSerialPort::StopBits stop_bits_ = QSerialPort::OneStop;
    QSerialPort::FlowControl flow_control_ = QSerialPort::NoFlowControl;

    QByteArray read_buffer_;
    static constexpr char FRAME_START = 0x7E;
    static constexpr char FRAME_END = 0x7D;
    static constexpr char ESCAPE_CHAR = 0x7C;
};

} // namespace ReliNet
