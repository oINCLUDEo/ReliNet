#include "itransport.hpp"
#include "serial_transport.cpp" // Include SerialTransport implementation
#include <QTimer>
#include <QRegularExpression>
#include <QDebug>

namespace ReliNet {

// Forward declaration for SerialTransport if not included
class SerialTransport;

class SatelliteTransport : public ITransport {
    Q_OBJECT
    
public:
    explicit SatelliteTransport(QObject* parent = nullptr);
    ~SatelliteTransport() override;
    
    // ITransport interface
    QString transportName() const override { return "Iridium Satellite Modem"; }
    TransportCapabilities capabilities() const override;
    TransportType transportType() const override { return TransportType::SATELLITE; }
    
    bool isConnected() const override;
    QString getConnectionInfo() const override;
    
    // Satellite-specific configuration
    void setModemPort(const QString& portName);
    void setSignalCheckInterval(int seconds) { signal_check_interval_ = seconds; }
    
    // Satellite modem status
    int getSignalStrength() const { return signal_strength_; }
    QString getModemStatus() const { return modem_status_; }
    
public slots:
    void connectToHost(const QString& address, uint16_t port) override;
    void disconnect() override;
    void sendData(const QByteArray& data) override;
    
signals:
    void signalStrengthChanged(int strength);
    void modemStatusChanged(const QString& status);
    void satelliteMessageSent();
    void satelliteMessageReceived();
    
private slots:
    void onSerialConnected();
    void onSerialDisconnected();
    void onSerialDataReceived(const QByteArray& data);
    void onSerialError(const QString& error, TransportError errorType);
    void onSignalCheckTimer();
    void onModemResponseTimeout();
    
private:
    enum ModemState {
        Idle,
        CheckingSignal,
        SendingMessage,
        ReceivingMessage,
        WaitingResponse
    };
    
    void initializeModem();
    void checkSignalStrength();
    void sendAtCommand(const QString& command);
    void processModemResponse(const QString& response);
    bool prepareMessageForTransmission(const QByteArray& data);
    
    SerialTransport* serial_transport_;
    QString modem_port_;
    QString destination_address_;
    
    // Satellite modem state
    ModemState modem_state_ = Idle;
    int signal_strength_ = 0;
    QString modem_status_ = "Offline";
    int signal_check_interval_ = 30; // seconds
    
    // Iridium SBD specific
    static constexpr int IRIDIUM_SBD_MTU = 340; // Iridium SBD maximum message size
    QByteArray pending_outbound_data_;
    
    // Timers and response handling
    QTimer* signal_check_timer_;
    QTimer* response_timeout_timer_;
    QString expected_response_;
    QByteArray response_buffer_;
};

SatelliteTransport::SatelliteTransport(QObject* parent) 
    : ITransport(parent), serial_transport_(new SerialTransport(this)) {
    
    // Configure serial for satellite modem (typical settings)
    serial_transport_->setBaudRate(19200);
    serial_transport_->setDataBits(QSerialPort::Data8);
    serial_transport_->setParity(QSerialPort::NoParity);
    serial_transport_->setStopBits(QSerialPort::OneStop);
    serial_transport_->setFlowControl(QSerialPort::HardwareControl);
    
    connect(serial_transport_, &ITransport::connected, this, &SatelliteTransport::onSerialConnected);
    connect(serial_transport_, &ITransport::disconnected, this, &SatelliteTransport::onSerialDisconnected);
    connect(serial_transport_, &ITransport::dataReceived, this, &SatelliteTransport::onSerialDataReceived);
    connect(serial_transport_, &ITransport::errorOccurred, this, &SatelliteTransport::onSerialError);
    
    // Signal strength check timer
    signal_check_timer_ = new QTimer(this);
    signal_check_timer_->setInterval(signal_check_interval_ * 1000);
    connect(signal_check_timer_, &QTimer::timeout, this, &SatelliteTransport::onSignalCheckTimer);
    
    // Response timeout timer
    response_timeout_timer_ = new QTimer(this);
    response_timeout_timer_->setSingleShot(true);
    response_timeout_timer_->setInterval(30000); // 30 second timeout
    connect(response_timeout_timer_, &QTimer::timeout, this, &SatelliteTransport::onModemResponseTimeout);
}

SatelliteTransport::~SatelliteTransport() = default;

TransportCapabilities SatelliteTransport::capabilities() const {
    TransportCapabilities caps;
    caps.multi_homing = false;
    caps.message_boundaries = true; // Satellite messages are discrete
    caps.mtu = IRIDIUM_SBD_MTU;
    caps.estimated_latency_ms = 20000; // 20+ seconds for satellite
    caps.encrypted = false;
    caps.reliable = false; // Satellite can fail due to atmospheric conditions
    caps.ordered = false;  // Messages might arrive out of order
    return caps;
}

bool SatelliteTransport::isConnected() const {
    return serial_transport_->isConnected() && (signal_strength_ > 0);
}

QString SatelliteTransport::getConnectionInfo() const {
    if (isConnected()) {
        return QString("Satellite Modem: Signal %1/5, %2").arg(signal_strength_).arg(modem_status_);
    }
    return QString("Satellite Modem: %1").arg(modem_status_);
}

void SatelliteTransport::setModemPort(const QString& portName) {
    modem_port_ = portName;
}

void SatelliteTransport::connectToHost(const QString& address, uint16_t port) {
    Q_UNUSED(port) // Satellite doesn't use traditional ports
    destination_address_ = address;
    
    if (modem_port_.isEmpty()) {
        emit errorOccurred("No satellite modem port configured", TransportError::InvalidAddress);
        return;
    }
    
    // Connect to satellite modem via serial
    serial_transport_->connectToHost(modem_port_, 19200);
}

void SatelliteTransport::disconnect() {
    signal_check_timer_->stop();
    response_timeout_timer_->stop();
    
    if (serial_transport_->isConnected()) {
        sendAtCommand("AT+SBDDET"); // Detach from satellite network
        serial_transport_->disconnect();
    }
    
    modem_status_ = "Offline";
    signal_strength_ = 0;
    emit modemStatusChanged(modem_status_);
}

void SatelliteTransport::sendData(const QByteArray& data) {
    if (!isConnected()) {
        emit errorOccurred("Cannot send data: satellite link not available", TransportError::NetworkUnreachable);
        return;
    }
    
    if (data.size() > IRIDIUM_SBD_MTU) {
        emit errorOccurred(QString("Message too large: %1 bytes (max %2)")
                          .arg(data.size()).arg(IRIDIUM_SBD_MTU), TransportError::BufferFull);
        return;
    }
    
    if (modem_state_ != Idle) {
        emit errorOccurred("Modem busy, cannot send message", TransportError::BufferFull);
        return;
    }
    
    if (!prepareMessageForTransmission(data)) {
        emit errorOccurred("Failed to prepare message for transmission", TransportError::TransportSpecific);
        return;
    }
}

void SatelliteTransport::onSerialConnected() {
    modem_status_ = "Initializing";
    emit modemStatusChanged(modem_status_);
    initializeModem();
}

void SatelliteTransport::onSerialDisconnected() {
    signal_check_timer_->stop();
    modem_status_ = "Offline";
    signal_strength_ = 0;
    emit modemStatusChanged(modem_status_);
    emit signalStrengthChanged(signal_strength_);
    emit disconnected();
    emit connectionStateChanged(false);
}

void SatelliteTransport::onSerialDataReceived(const QByteArray& data) {
    response_buffer_.append(data);
    
    // Process complete lines
    while (response_buffer_.contains('\n')) {
        int line_end = response_buffer_.indexOf('\n');
        QByteArray line = response_buffer_.left(line_end);
        response_buffer_.remove(0, line_end + 1);
        
        QString response = QString::fromLatin1(line).trimmed();
        if (!response.isEmpty()) {
            processModemResponse(response);
        }
    }
}

void SatelliteTransport::onSerialError(const QString& error, TransportError errorType) {
    emit errorOccurred("Satellite modem error: " + error, errorType);
}

void SatelliteTransport::onSignalCheckTimer() {
    if (modem_state_ == Idle) {
        checkSignalStrength();
    }
}

void SatelliteTransport::onModemResponseTimeout() {
    modem_state_ = Idle;
    emit errorOccurred("Satellite modem response timeout", TransportError::Timeout);
}

void SatelliteTransport::initializeModem() {
    modem_state_ = WaitingResponse;
    expected_response_ = "OK";
    
    // Basic AT commands to initialize Iridium modem
    sendAtCommand("ATE0"); // Echo off
    sendAtCommand("AT&K0"); // Flow control off  
    sendAtCommand("AT+SBDMTA=0"); // Disable mobile terminated alerts
    
    // Start signal monitoring
    signal_check_timer_->start();
    checkSignalStrength();
}

void SatelliteTransport::checkSignalStrength() {
    if (modem_state_ != Idle) return;
    
    modem_state_ = CheckingSignal;
    expected_response_ = "+CSQ:";
    sendAtCommand("AT+CSQ"); // Check signal quality
}

void SatelliteTransport::sendAtCommand(const QString& command) {
    if (!serial_transport_->isConnected()) return;
    
    QString full_command = command + "\r\n";
    serial_transport_->sendData(full_command.toLatin1());
    response_timeout_timer_->start();
}

void SatelliteTransport::processModemResponse(const QString& response) {
    response_timeout_timer_->stop();
    
    if (response == "OK") {
        if (modem_state_ == WaitingResponse) {
            modem_status_ = "Ready";
            emit modemStatusChanged(modem_status_);
            emit connected();
            emit connectionStateChanged(true);
        }
        modem_state_ = Idle;
        return;
    }
    
    if (response.startsWith("+CSQ:")) {
        // Parse signal strength: +CSQ:<signal>,<error_rate>
        QRegularExpression regex(R"(\+CSQ:(\d+),(\d+))");
        auto match = regex.match(response);
        if (match.hasMatch()) {
            int signal = match.captured(1).toInt();
            // Convert to 0-5 scale (31 is max signal in AT+CSQ)
            signal_strength_ = (signal >= 31) ? 0 : (signal / 6); // 0=no signal, 5=full
            emit signalStrengthChanged(signal_strength_);
        }
        modem_state_ = Idle;
        return;
    }
    
    if (response.startsWith("+SBDIX:")) {
        // SBD session status: +SBDIX:<MO status>,<MOMSN>,<MT status>,<MTMSN>,<MT length>,<MT queued>
        QRegularExpression regex(R"(\+SBDIX:(\d+),\d+,(\d+),\d+,(\d+),\d+)");
        auto match = regex.match(response);
        if (match.hasMatch()) {
            int mo_status = match.captured(1).toInt();
            int mt_status = match.captured(2).toInt();
            int mt_length = match.captured(3).toInt();
            
            if (mo_status >= 0 && mo_status <= 4) {
                emit satelliteMessageSent();
                modem_status_ = "Message sent successfully";
            } else {
                emit errorOccurred("Satellite message send failed", TransportError::TransportSpecific);
            }
            
            if (mt_status == 1 && mt_length > 0) {
                // Received message available
                sendAtCommand("AT+SBDRB"); // Read binary data
            }
        }
        modem_state_ = Idle;
        return;
    }
    
    if (response == "ERROR") {
        modem_state_ = Idle;
        emit errorOccurred("Satellite modem command error", TransportError::TransportSpecific);
        return;
    }
    
    // Handle binary data response (simplified)
    if (response_buffer_.size() > 2 && modem_state_ == ReceivingMessage) {
        // In a real implementation, we would properly parse the binary SBD response
        // For now, treat any binary data as a received message
        emit dataReceived(response_buffer_);
        emit satelliteMessageReceived();
        response_buffer_.clear();
        modem_state_ = Idle;
    }
}

bool SatelliteTransport::prepareMessageForTransmission(const QByteArray& data) {
    if (data.size() > IRIDIUM_SBD_MTU) {
        return false;
    }
    
    pending_outbound_data_ = data;
    modem_state_ = SendingMessage;
    expected_response_ = "OK";
    
    // Load message into modem buffer (simplified - should use binary mode)
    QString hex_data = data.toHex();
    sendAtCommand(QString("AT+SBDWT=%1").arg(hex_data));
    
    // Initiate SBD session
    sendAtCommand("AT+SBDIX");
    
    return true;
}

} // namespace ReliNet

#include "satellite_transport.moc"