#pragma once

#include "protocol.hpp"
#include <QObject>
#include <QTimer>

namespace ReliNet {

class Heartbeat : public QObject {
    Q_OBJECT
    
public:
    explicit Heartbeat(QObject* parent = nullptr);
    
    void setInterval(int seconds) { interval_seconds_ = seconds; }
    void start();
    void stop();
    void handlePong(uint64_t timestamp);
    
signals:
    void sendPing(const QByteArray& ping_data);
    void heartbeatFailure();
    void rttMeasured(int rtt_ms);
    
private slots:
    void onPingTimer();
    void onTimeoutTimer();
    
private:
    QTimer* ping_timer_;
    QTimer* timeout_timer_;
    int interval_seconds_ = 10;
    uint64_t last_ping_timestamp_ = 0;
    int missed_pongs_ = 0;
    static constexpr int MAX_MISSED_PONGS = 3;
};

} // namespace ReliNet