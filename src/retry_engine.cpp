#include "retry_engine.hpp"
#include <QRandomGenerator>
#include <QDebug>
#include <algorithm>

namespace ReliNet {

InFlightMessage::InFlightMessage(const Message& msg) 
    : message(msg), send_timestamp(QDateTime::currentDateTime()), retry_timer(std::make_unique<QTimer>()) {
    retry_timer->setSingleShot(true);
}

InFlightMessage::InFlightMessage(InFlightMessage&& other) noexcept
    : message(std::move(other.message))
    , send_timestamp(other.send_timestamp)
    , retry_count(other.retry_count)
    , retry_timer(std::move(other.retry_timer)) {
}

InFlightMessage& InFlightMessage::operator=(InFlightMessage&& other) noexcept {
    if (this != &other) {
        message = std::move(other.message);
        send_timestamp = other.send_timestamp;
        retry_count = other.retry_count;
        retry_timer = std::move(other.retry_timer);
    }
    return *this;
}

RetryEngine::RetryEngine(QObject* parent) : QObject(parent) {
    // Cleanup old deduplication entries periodically
    auto* cleanup_timer = new QTimer(this);
    connect(cleanup_timer, &QTimer::timeout, [this]() {
        if (received_message_ids_.size() > DEDUP_HISTORY_SIZE) {
            // Keep only the most recent half
            QList<uint64_t> ids(received_message_ids_.begin(), received_message_ids_.end());
            std::sort(ids.rbegin(), ids.rend()); // Newest first (assuming higher IDs are newer)
            received_message_ids_.clear();
            for (int i = 0; i < DEDUP_HISTORY_SIZE / 2; ++i) {
                received_message_ids_.insert(ids[i]);
            }
        }
    });
    cleanup_timer->start(60000); // Every minute
}

RetryEngine::~RetryEngine() = default;

void RetryEngine::setConfig(const RetryConfig& config) {
    config_ = config;
}

void RetryEngine::sendMessage(const Message& message) {
    // If this is SOS/DISTRESS, it should jump to front of processing
    uint64_t msg_id = message.header.message_id;
    
    // Create in-flight record
    auto in_flight = std::make_unique<InFlightMessage>(message);
    
    // Connect retry timer
    connect(in_flight->retry_timer.get(), &QTimer::timeout, this, &RetryEngine::onRetryTimer);
    
    // Store in map using emplace
    in_flight_messages_.emplace(msg_id, std::move(in_flight));
    
    // Serialize and send immediately
    auto serialized = ProtocolCodec::serialize(message);
    if (serialized) {
        emit sendRequest(*serialized);
        scheduleRetry(msg_id);
    } else {
        // Serialization failed - treat as immediate delivery failure
        removeInFlightMessage(msg_id);
        emit deliveryFailed(msg_id, message.header.priority);
    }
}

void RetryEngine::handleAck(uint64_t message_id) {
    auto it = in_flight_messages_.find(message_id);
    if (it != in_flight_messages_.end()) {
        Priority priority = it->second->message.header.priority;
        removeInFlightMessage(message_id);
        emit deliveryConfirmed(message_id, priority);
    }
}

bool RetryEngine::shouldProcessMessage(uint64_t message_id) {
    if (received_message_ids_.contains(message_id)) {
        emit messageDeduplicationRequired(message_id);
        return false; // Duplicate
    }
    
    received_message_ids_.insert(message_id);
    return true;
}

void RetryEngine::forceRetry(uint64_t message_id) {
    auto it = in_flight_messages_.find(message_id);
    if (it != in_flight_messages_.end()) {
        // Stop current timer and retry immediately
        it->second->retry_timer->stop();
        onRetryTimer(); // This will find the right message by sender()
    }
}

void RetryEngine::cancelMessage(uint64_t message_id) {
    auto it = in_flight_messages_.find(message_id);
    if (it != in_flight_messages_.end()) {
        Priority priority = it->second->message.header.priority;
        removeInFlightMessage(message_id);
        emit deliveryFailed(message_id, priority); // Treat cancellation as failure
    }
}

QList<uint64_t> RetryEngine::getInFlightMessageIds() const {
    QList<uint64_t> keys;
    keys.reserve(static_cast<int>(in_flight_messages_.size()));
    for (const auto& kv : in_flight_messages_) {
        keys.append(kv.first);
    }
    return keys;
}

void RetryEngine::onRetryTimer() {
    // Find which message triggered this timeout
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    
    uint64_t message_id = 0;
    auto it = std::find_if(in_flight_messages_.begin(), in_flight_messages_.end(),
                          [timer](const std::pair<const uint64_t, std::unique_ptr<InFlightMessage>>& kv) {
                              return kv.second->retry_timer.get() == timer;
                          });
    
    if (it == in_flight_messages_.end()) return;
    
    message_id = it->first;
    InFlightMessage& in_flight = *(it->second);
    
    Priority priority = in_flight.message.header.priority;
    int max_retries = getMaxRetries(priority);
    
    // Check if we've exceeded retry limit (unless SOS/DISTRESS with unlimited retries)
    if (max_retries >= 0 && in_flight.retry_count >= max_retries) {
        removeInFlightMessage(message_id);
        emit deliveryFailed(message_id, priority);
        return;
    }
    
    // Increment retry count and attempt resend
    in_flight.retry_count++;
    emit retryAttempt(message_id, in_flight.retry_count, priority);
    
    // Serialize and send
    auto serialized = ProtocolCodec::serialize(in_flight.message);
    if (serialized) {
        emit sendRequest(*serialized);
        scheduleRetry(message_id);
    } else {
        // Serialization failed
        removeInFlightMessage(message_id);
        emit deliveryFailed(message_id, priority);
    }
}

int RetryEngine::calculateBackoffMs(Priority priority, int retry_count) {
    int base_ms = config_.base_backoff_ms;
    int max_ms = getMaxBackoffMs(priority);
    
    // Exponential backoff: base * 2^retry_count, capped at max
    int backoff_ms = base_ms << std::min(retry_count, 6); // Cap exponent to prevent overflow
    backoff_ms = std::min(backoff_ms, max_ms);
    
    // Add jitter: ±jitter_percent%
    int jitter_range = (backoff_ms * config_.jitter_percent) / 100;
    int jitter = QRandomGenerator::global()->bounded(-jitter_range, jitter_range + 1);
    
    return std::max(100, backoff_ms + jitter); // Minimum 100ms
}

int RetryEngine::getMaxRetries(Priority priority) {
    if (ProtocolCodec::isPriorityEmergency(priority)) {
        return config_.sos_max_retries; // Unlimited (-1) for SOS/DISTRESS
    } else if (priority == Priority::URGENT) {
        return config_.urgent_max_retries;
    } else {
        return config_.routine_max_retries;
    }
}

int RetryEngine::getMaxBackoffMs(Priority priority) {
    if (ProtocolCodec::isPriorityEmergency(priority)) {
        return config_.sos_max_backoff_ms;
    } else if (priority == Priority::URGENT) {
        return config_.urgent_max_backoff_ms;
    } else {
        return config_.max_backoff_ms;
    }
}

void RetryEngine::scheduleRetry(uint64_t message_id) {
    auto it = in_flight_messages_.find(message_id);
    if (it == in_flight_messages_.end()) return;
    
    InFlightMessage& in_flight = *(it->second);
    int backoff_ms = calculateBackoffMs(in_flight.message.header.priority, in_flight.retry_count);
    
    in_flight.retry_timer->start(backoff_ms);
}

void RetryEngine::removeInFlightMessage(uint64_t message_id) {
    auto it = in_flight_messages_.find(message_id);
    if (it != in_flight_messages_.end()) {
        it->second->retry_timer->stop();
        in_flight_messages_.erase(it);
    }
}

} // namespace ReliNet

#include "retry_engine.moc"