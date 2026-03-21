#pragma once

#include <QObject>

namespace ReliNet {

enum class LinkState {
    GOOD,     // RTT < 500ms, loss < 2%
    DEGRADED, // RTT 500-3000ms, loss 2-15%
    POOR      // RTT > 3000ms or loss > 15%
};

class LinkMonitor : public QObject {
    Q_OBJECT
    
public:
    explicit LinkMonitor(QObject* parent = nullptr);
    
    void recordRtt(int rtt_ms);
    void recordMessageSent();
    void recordMessageLost();
    
    LinkState getLinkState() const { return current_state_; }
    float getLossRate() const;
    int getAverageRtt() const;
    
signals:
    void linkStateChanged(LinkState new_state, int avg_rtt_ms, float loss_percent);
    
private:
    void updateLinkState();
    
    LinkState current_state_ = LinkState::GOOD;
    QList<int> rtt_history_;
    int messages_sent_ = 0;
    int messages_lost_ = 0;
    static constexpr int HISTORY_SIZE = 20;
};

class AdaptiveSender : public QObject {
    Q_OBJECT
    
public:
    explicit AdaptiveSender(QObject* parent = nullptr);
    
    bool shouldBatch(const class Message& message, LinkState link_state);
    
private:
    // Batching logic based on link state
};

} // namespace ReliNet