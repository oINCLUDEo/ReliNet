#include "link_monitor.hpp"
#include "protocol.hpp"
#include <numeric>

namespace ReliNet {

LinkMonitor::LinkMonitor(QObject* parent) : QObject(parent) {}

void LinkMonitor::recordRtt(int rtt_ms) {
    rtt_history_.append(rtt_ms);
    if (rtt_history_.size() > HISTORY_SIZE) {
        rtt_history_.removeFirst();
    }
    updateLinkState();
}

void LinkMonitor::recordMessageSent() {
    messages_sent_++;
}

void LinkMonitor::recordMessageLost() {
    messages_lost_++;
    updateLinkState();
}

float LinkMonitor::getLossRate() const {
    if (messages_sent_ == 0) return 0.0f;
    return static_cast<float>(messages_lost_) / messages_sent_ * 100.0f;
}

int LinkMonitor::getAverageRtt() const {
    if (rtt_history_.isEmpty()) return 0;
    return std::accumulate(rtt_history_.begin(), rtt_history_.end(), 0) / rtt_history_.size();
}

void LinkMonitor::updateLinkState() {
    int avg_rtt = getAverageRtt();
    float loss_rate = getLossRate();
    
    LinkState new_state;
    if (avg_rtt < 500 && loss_rate < 2.0f) {
        new_state = LinkState::GOOD;
    } else if (avg_rtt < 3000 && loss_rate < 15.0f) {
        new_state = LinkState::DEGRADED;
    } else {
        new_state = LinkState::POOR;
    }
    
    if (new_state != current_state_) {
        current_state_ = new_state;
        emit linkStateChanged(new_state, avg_rtt, loss_rate);
    }
}

AdaptiveSender::AdaptiveSender(QObject* parent) : QObject(parent) {}

bool AdaptiveSender::shouldBatch(const Message& message, LinkState link_state) {
    // SOS/DISTRESS always bypass batching
    if (ProtocolCodec::isPriorityEmergency(message.header.priority)) {
        return false;
    }
    
    // URGENT bypasses batching only in POOR state
    if (message.header.priority == Priority::URGENT && link_state != LinkState::POOR) {
        return false;
    }
    
    // Batch in DEGRADED or POOR states
    return link_state != LinkState::GOOD;
}

} // namespace ReliNet

#include "adaptive_sender.moc"