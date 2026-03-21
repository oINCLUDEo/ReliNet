#include "heartbeat.hpp"
#include <QDateTime>

namespace ReliNet {

Heartbeat::Heartbeat(QObject* parent) : QObject(parent) {
    ping_timer_ = new QTimer(this);
    timeout_timer_ = new QTimer(this);
    timeout_timer_->setSingleShot(true);
    
    connect(ping_timer_, &QTimer::timeout, this, &Heartbeat::onPingTimer);
    connect(timeout_timer_, &QTimer::timeout, this, &Heartbeat::onTimeoutTimer);
}

void Heartbeat::start() {
    ping_timer_->start(interval_seconds_ * 1000);
    missed_pongs_ = 0;
}

void Heartbeat::stop() {
    ping_timer_->stop();
    timeout_timer_->stop();
}

void Heartbeat::handlePong(uint64_t timestamp) {
    if (timestamp == last_ping_timestamp_) {
        timeout_timer_->stop();
        int rtt_ms = QDateTime::currentMSecsSinceEpoch() - timestamp;
        emit rttMeasured(rtt_ms);
        missed_pongs_ = 0;
    }
}

void Heartbeat::onPingTimer() {
    last_ping_timestamp_ = QDateTime::currentMSecsSinceEpoch();
    
    HeartbeatMessage ping;
    ping.type = HeartbeatMessage::PING;
    ping.timestamp = last_ping_timestamp_;
    
    emit sendPing(ping.serialize());
    timeout_timer_->start(interval_seconds_ * 3 * 1000); // 3x interval timeout
}

void Heartbeat::onTimeoutTimer() {
    missed_pongs_++;
    if (missed_pongs_ >= MAX_MISSED_PONGS) {
        emit heartbeatFailure();
    }
}

} // namespace ReliNet

#include "heartbeat.moc"