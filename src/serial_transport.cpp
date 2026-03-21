#include "itransport.hpp"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>

namespace ReliNet {

class SerialTransport : public ITransport {
    Q_OBJECT
    
public:
    explicit SerialTransport(QObject* parent = nullptr);
    ~SerialTransport() override;
    
    // ITransport interface
    QString transportName() const override { return "Serial Port"; }
    TransportCapabilities capabilities() const override;
    TransportType transportType() const override { return TransportType::SERIAL; }
    
    bool isConnected() const override;
    QString getConnectionInfo() const override;
    
    // Serial-specific configuration
    void setBaudRate(qint32 baudRate) { baud_rate_ = baudRate; }
    void setDataBits(QSerialPort::DataBits dataBits) { data_bits_ = dataBits; }
    void setParity(QSerialPort::Parity parity) { parity_ = parity; }
    void setStopBits(QSerialPort::StopBits stopBits) { stop_bits_ = stopBits; }
    void setFlowControl(QSerialPort::FlowControl flowControl) { flow_control_ = flowControl; }
    
    // List available serial ports
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
    
    // Serial configuration
    qint32 baud_rate_ = 9600;
    QSerialPort::DataBits data_bits_ = QSerialPort::Data8;
    QSerialPort::Parity parity_ = QSerialPort::NoParity;
    QSerialPort::StopBits stop_bits_ = QSerialPort::OneStop;
    QSerialPort::FlowControl flow_control_ = QSerialPort::NoFlowControl;
    
    // Frame detection for serial streams
    QByteArray read_buffer_;
    static constexpr char FRAME_START = 0x7E;
    static constexpr char FRAME_END = 0x7D;
    static constexpr char ESCAPE_CHAR = 0x7C;
};

SerialTransport::SerialTransport(QObject* parent) 
    : ITransport(parent), serial_port_(new QSerialPort(this)) {
    
    connect(serial_port_, &QSerialPort::readyRead, this, &SerialTransport::onSerialReadyRead);
    connect(serial_port_, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, &SerialTransport::onSerialError);
}

SerialTransport::~SerialTransport() {
    if (serial_port_->isOpen()) {
        serial_port_->close();
    }
}

TransportCapabilities SerialTransport::capabilities() const {
    TransportCapabilities caps;
    caps.multi_homing = false;
    caps.message_boundaries = false; // Serial is a stream
    caps.mtu = 256; // Conservative MTU for serial
    caps.estimated_latency_ms = 100; // Depends on baud rate
    caps.encrypted = false;
    caps.reliable = false; // Serial can lose data
    caps.ordered = true;   // Serial preserves order
    return caps;
}

bool SerialTransport::isConnected() const {
    return serial_port_->isOpen();
}

QString SerialTransport::getConnectionInfo() const {
    if (isConnected()) {
        return QString("Serial %1 @ %2 bps").arg(port_name_).arg(baud_rate_);
    }
    return "Serial Disconnected";
}

QStringList SerialTransport::availablePorts() {
    QStringList ports;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        ports.append(info.portName());
    }
    return ports;
}

void SerialTransport::connectToHost(const QString& portName, uint16_t baudRate) {
    port_name_ = portName;
    baud_rate_ = baudRate;
    
    if (serial_port_->isOpen()) {
        serial_port_->close();
    }
    
    serial_port_->setPortName(portName);
    serial_port_->setBaudRate(baud_rate_);
    serial_port_->setDataBits(data_bits_);
    serial_port_->setParity(parity_);
    serial_port_->setStopBits(stop_bits_);
    serial_port_->setFlowControl(flow_control_);
    
    if (serial_port_->open(QIODevice::ReadWrite)) {
        emit connected();
        emit connectionStateChanged(true);
    } else {
        emit errorOccurred("Failed to open serial port: " + serial_port_->errorString(),
                          TransportError::ConnectionFailed);
    }
}

void SerialTransport::disconnect() {
    if (serial_port_->isOpen()) {
        serial_port_->close();
        emit disconnected();
        emit connectionStateChanged(false);
    }
}

void SerialTransport::sendData(const QByteArray& data) {
    if (!isConnected()) {
        emit errorOccurred("Cannot send data: not connected", TransportError::ConnectionFailed);
        return;
    }
    
    // Frame the data for reliable transmission over serial
    QByteArray framed_data;
    framed_data.append(FRAME_START);
    
    // Escape special characters in payload
    for (char byte : data) {
        if (byte == FRAME_START || byte == FRAME_END || byte == ESCAPE_CHAR) {
            framed_data.append(ESCAPE_CHAR);
        }
        framed_data.append(byte);
    }
    
    framed_data.append(FRAME_END);
    
    qint64 written = serial_port_->write(framed_data);
    if (written != framed_data.size()) {
        emit errorOccurred("Failed to send all data over serial", TransportError::TransportSpecific);
    }
}

void SerialTransport::onSerialReadyRead() {
    QByteArray new_data = serial_port_->readAll();
    read_buffer_.append(new_data);
    
    // Process frames in buffer
    while (true) {
        int start_pos = read_buffer_.indexOf(FRAME_START);
        if (start_pos == -1) {
            // No frame start found, discard everything
            read_buffer_.clear();
            break;
        }
        
        // Remove data before frame start
        if (start_pos > 0) {
            read_buffer_.remove(0, start_pos);
        }
        
        int end_pos = read_buffer_.indexOf(FRAME_END, 1); // Start searching after frame start
        if (end_pos == -1) {
            // No complete frame yet
            break;
        }
        
        // Extract frame (excluding start/end markers)
        QByteArray frame_data = read_buffer_.mid(1, end_pos - 1);
        read_buffer_.remove(0, end_pos + 1);
        
        // Unescape the frame data
        QByteArray unescaped_data;
        bool escaped = false;
        for (char byte : frame_data) {
            if (escaped) {
                unescaped_data.append(byte);
                escaped = false;
            } else if (byte == ESCAPE_CHAR) {
                escaped = true;
            } else {
                unescaped_data.append(byte);
            }
        }
        
        if (!unescaped_data.isEmpty()) {
            emit dataReceived(unescaped_data);
        }
    }
}

void SerialTransport::onSerialError(QSerialPort::SerialPortError error) {
    QString errorString;
    TransportError transportError = TransportError::TransportSpecific;
    
    switch (error) {
    case QSerialPort::DeviceNotFoundError:
        errorString = "Serial device not found";
        transportError = TransportError::NetworkUnreachable;
        break;
    case QSerialPort::PermissionError:
        errorString = "Permission denied on serial port";
        transportError = TransportError::AuthenticationFailed;
        break;
    case QSerialPort::OpenError:
        errorString = "Failed to open serial port";
        transportError = TransportError::ConnectionFailed;
        break;
    case QSerialPort::WriteError:
        errorString = "Serial write error";
        transportError = TransportError::TransportSpecific;
        break;
    case QSerialPort::ReadError:
        errorString = "Serial read error";
        transportError = TransportError::TransportSpecific;
        break;
    case QSerialPort::ResourceError:
        errorString = "Serial resource error";
        transportError = TransportError::NetworkUnreachable;
        break;
    case QSerialPort::UnsupportedOperationError:
        errorString = "Unsupported serial operation";
        transportError = TransportError::NotImplemented;
        break;
    case QSerialPort::TimeoutError:
        errorString = "Serial timeout";
        transportError = TransportError::Timeout;
        break;
    default:
        if (error != QSerialPort::NoError) {
            errorString = "Unknown serial error";
        }
        break;
    }
    
    if (!errorString.isEmpty()) {
        emit errorOccurred(errorString, transportError);
    }
}

} // namespace ReliNet

#include "serial_transport.moc"