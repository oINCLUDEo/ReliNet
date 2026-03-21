#include "itransport.hpp"
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QStringList>
#include <QSocketNotifier>
#include <QHostAddress>
#include <QDebug>

// SCTP headers (if available)
#ifdef HAVE_SCTP
#include <sys/socket.h>
#include <netinet/sctp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace ReliNet {

class SctpWorker;

class SctpTransport : public ITransport {
    Q_OBJECT
    
public:
    explicit SctpTransport(QObject* parent = nullptr);
    ~SctpTransport() override;
    
    // ITransport interface
    QString transportName() const override { return "SCTP Multi-homed"; }
    TransportCapabilities capabilities() const override;
    TransportType transportType() const override { return TransportType::SCTP; }
    
    bool isConnected() const override;
    QString getConnectionInfo() const override;
    
    // Multi-homing support
    void setMultipleAddresses(const QStringList& local_addresses, 
                             const QStringList& remote_addresses) override;
    
    // SCTP-specific
    void setStream(int stream_id) override;
    int getActivePathCount() const { return active_paths_; }
    
public slots:
    void connectToHost(const QString& address, uint16_t port) override;
    void disconnect() override;
    void sendData(const QByteArray& data) override;
    
signals:
    void pathChanged(const QString& old_path, const QString& new_path);
    void associationChanged(const QString& event);
    
private slots:
    void onWorkerConnected();
    void onWorkerDisconnected();
    void onWorkerDataReceived(const QByteArray& data);
    void onWorkerError(const QString& error);
    void onPathStatusChanged(int active_paths);
    
private:
    SctpWorker* worker_;
    QThread* worker_thread_;
    
    QStringList local_addresses_;
    QStringList remote_addresses_;
    uint16_t remote_port_ = 0;
    int current_stream_ = 0;
    int active_paths_ = 0;
    bool connected_ = false;
    
    mutable QMutex state_mutex_;
};

class SctpWorker : public QObject {
    Q_OBJECT
    
public:
    explicit SctpWorker(QObject* parent = nullptr);
    ~SctpWorker();
    
    void setAddresses(const QStringList& local, const QStringList& remote, uint16_t port);
    void setStream(int stream_id) { current_stream_ = stream_id; }
    
public slots:
    void connectToHost();
    void disconnect();
    void sendData(const QByteArray& data);
    
signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray& data);
    void errorOccurred(const QString& error);
    void pathStatusChanged(int active_paths);
    
private slots:
    void onSocketActivated(int socket);
    
private:
    bool setupSocket();
    void closeSocket();
    void processNotifications();
    
    int socket_fd_ = -1;
    QSocketNotifier* socket_notifier_ = nullptr;
    QStringList local_addresses_;
    QStringList remote_addresses_;
    uint16_t remote_port_ = 0;
    int current_stream_ = 0;
    bool connected_ = false;
};

// SctpTransport Implementation
SctpTransport::SctpTransport(QObject* parent) 
    : ITransport(parent), worker_(nullptr), worker_thread_(nullptr) {
    
#ifdef HAVE_SCTP
    worker_ = new SctpWorker();
    worker_thread_ = new QThread(this);
    worker_->moveToThread(worker_thread_);
    
    connect(worker_, &SctpWorker::connected, this, &SctpTransport::onWorkerConnected);
    connect(worker_, &SctpWorker::disconnected, this, &SctpTransport::onWorkerDisconnected);
    connect(worker_, &SctpWorker::dataReceived, this, &SctpTransport::onWorkerDataReceived);
    connect(worker_, &SctpWorker::errorOccurred, this, &SctpTransport::onWorkerError);
    connect(worker_, &SctpWorker::pathStatusChanged, this, &SctpTransport::onPathStatusChanged);
    
    worker_thread_->start();
#endif
}

SctpTransport::~SctpTransport() {
    if (worker_thread_) {
        worker_thread_->quit();
        worker_thread_->wait();
        delete worker_;
    }
}

TransportCapabilities SctpTransport::capabilities() const {
    TransportCapabilities caps;
    caps.multi_homing = true;
    caps.message_boundaries = true;
    caps.mtu = 1452; // Typical SCTP MTU
    caps.estimated_latency_ms = 40;
    caps.encrypted = false;
    caps.reliable = true;
    caps.ordered = true;
    return caps;
}

bool SctpTransport::isConnected() const {
    QMutexLocker locker(&state_mutex_);
    return connected_;
}

QString SctpTransport::getConnectionInfo() const {
    QMutexLocker locker(&state_mutex_);
    if (connected_) {
        return QString("SCTP %1 paths, stream %2")
               .arg(active_paths_)
               .arg(current_stream_);
    }
    return "SCTP Disconnected";
}

void SctpTransport::setMultipleAddresses(const QStringList& local_addresses, 
                                        const QStringList& remote_addresses) {
    local_addresses_ = local_addresses;
    remote_addresses_ = remote_addresses;
    
    if (worker_) {
        worker_->setAddresses(local_addresses, remote_addresses, remote_port_);
    }
}

void SctpTransport::setStream(int stream_id) {
    current_stream_ = stream_id;
    if (worker_) {
        worker_->setStream(stream_id);
    }
}

void SctpTransport::connectToHost(const QString& address, uint16_t port) {
#ifndef HAVE_SCTP
    emit errorOccurred("SCTP not available on this system", TransportError::NotImplemented);
    return;
#endif
    
    if (remote_addresses_.isEmpty()) {
        remote_addresses_ = QStringList{address};
    }
    remote_port_ = port;
    
    if (worker_) {
        worker_->setAddresses(local_addresses_, remote_addresses_, port);
        QMetaObject::invokeMethod(worker_, "connectToHost", Qt::QueuedConnection);
    }
}

void SctpTransport::disconnect() {
    if (worker_) {
        QMetaObject::invokeMethod(worker_, "disconnect", Qt::QueuedConnection);
    }
}

void SctpTransport::sendData(const QByteArray& data) {
    if (!isConnected()) {
        emit errorOccurred("Cannot send data: not connected", TransportError::ConnectionFailed);
        return;
    }
    
    if (worker_) {
        QMetaObject::invokeMethod(worker_, "sendData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
    }
}

void SctpTransport::onWorkerConnected() {
    QMutexLocker locker(&state_mutex_);
    connected_ = true;
    emit connected();
    emit connectionStateChanged(true);
}

void SctpTransport::onWorkerDisconnected() {
    QMutexLocker locker(&state_mutex_);
    connected_ = false;
    emit disconnected();
    emit connectionStateChanged(false);
}

void SctpTransport::onWorkerDataReceived(const QByteArray& data) {
    emit dataReceived(data);
}

void SctpTransport::onWorkerError(const QString& error) {
    emit errorOccurred(error, TransportError::TransportSpecific);
}

void SctpTransport::onPathStatusChanged(int active_paths) {
    QMutexLocker locker(&state_mutex_);
    active_paths_ = active_paths;
}

// SctpWorker Implementation
SctpWorker::SctpWorker(QObject* parent) : QObject(parent) {}

SctpWorker::~SctpWorker() {
    closeSocket();
}

void SctpWorker::setAddresses(const QStringList& local, const QStringList& remote, uint16_t port) {
    local_addresses_ = local;
    remote_addresses_ = remote;
    remote_port_ = port;
}

void SctpWorker::connectToHost() {
#ifdef HAVE_SCTP
    if (!setupSocket()) {
        emit errorOccurred("Failed to create SCTP socket");
        return;
    }
    
    // For this stub implementation, just emit connected after a short delay
    // In a real implementation, you would:
    // 1. Create sockaddr structures for all local/remote addresses
    // 2. Call sctp_connectx() for multi-homing
    // 3. Enable SCTP events for notifications
    // 4. Set up socket notifier for async I/O
    
    connected_ = true;
    emit connected();
    emit pathStatusChanged(remote_addresses_.size());
#else
    emit errorOccurred("SCTP support not compiled in");
#endif
}

void SctpWorker::disconnect() {
    closeSocket();
    if (connected_) {
        connected_ = false;
        emit disconnected();
    }
}

void SctpWorker::sendData(const QByteArray& data) {
#ifdef HAVE_SCTP
    if (!connected_ || socket_fd_ < 0) {
        emit errorOccurred("Cannot send: not connected");
        return;
    }
    
    // Use sctp_sendmsg with stream specification
    // This is a simplified stub - real implementation would use proper SCTP API
    ssize_t sent = send(socket_fd_, data.constData(), data.size(), 0);
    if (sent < 0) {
        emit errorOccurred("SCTP send failed");
    }
#else
    Q_UNUSED(data)
    emit errorOccurred("SCTP not available");
#endif
}

void SctpWorker::onSocketActivated(int socket) {
    Q_UNUSED(socket)
    // Handle incoming data and notifications
    char buffer[8192];
    
#ifdef HAVE_SCTP
    ssize_t received = recv(socket_fd_, buffer, sizeof(buffer), 0);
    if (received > 0) {
        emit dataReceived(QByteArray(buffer, received));
    } else if (received < 0) {
        emit errorOccurred("SCTP receive failed");
    }
#endif
}

bool SctpWorker::setupSocket() {
#ifdef HAVE_SCTP
    socket_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (socket_fd_ < 0) {
        return false;
    }
    
    // Enable SCTP events
    struct sctp_event_subscribe events;
    memset(&events, 0, sizeof(events));
    events.sctp_data_io_event = 1;
    events.sctp_association_event = 1;
    events.sctp_address_event = 1;
    events.sctp_peer_error_event = 1;
    
    if (setsockopt(socket_fd_, SOL_SCTP, SCTP_EVENTS, &events, sizeof(events)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    // Set up socket notifier
    socket_notifier_ = new QSocketNotifier(socket_fd_, QSocketNotifier::Read, this);
    connect(socket_notifier_, &QSocketNotifier::activated, this, &SctpWorker::onSocketActivated);
    
    return true;
#else
    return false;
#endif
}

void SctpWorker::closeSocket() {
    if (socket_notifier_) {
        delete socket_notifier_;
        socket_notifier_ = nullptr;
    }
    
    if (socket_fd_ >= 0) {
#ifdef HAVE_SCTP
        close(socket_fd_);
#endif
        socket_fd_ = -1;
    }
}

} // namespace ReliNet

#include "sctp_transport.moc"