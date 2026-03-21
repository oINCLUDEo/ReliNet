#pragma once

#include <QObject>
#include <QDateTime>

namespace ReliNet {

struct DeliveryStats {
    int messages_sent = 0;
    int messages_acked = 0;
    int messages_failed = 0;
    int retries_total = 0;
    int queue_depth = 0;
    int rtt_ms = 0;
    QString link_state;
    QString active_endpoint;
    int failover_count = 0;
    int sctp_path_switches = 0;
};

class Metrics : public QObject {
    Q_OBJECT
    
public:
    explicit Metrics(QObject* parent = nullptr);
    
    void incrementMessagesSent() { stats_.messages_sent++; }
    void incrementMessagesAcked() { stats_.messages_acked++; }
    void incrementMessagesFailed() { stats_.messages_failed++; }
    void incrementRetries() { stats_.retries_total++; }
    void setQueueDepth(int depth) { stats_.queue_depth = depth; }
    void setRtt(int rtt_ms) { stats_.rtt_ms = rtt_ms; }
    void setLinkState(const QString& state) { stats_.link_state = state; }
    void setActiveEndpoint(const QString& endpoint) { stats_.active_endpoint = endpoint; }
    void incrementFailovers() { stats_.failover_count++; }
    void incrementSctpPathSwitches() { stats_.sctp_path_switches++; }
    
    DeliveryStats getStats() const { return stats_; }
    
signals:
    void statsUpdated(const DeliveryStats& stats);
    
private:
    DeliveryStats stats_;
};

} // namespace ReliNet