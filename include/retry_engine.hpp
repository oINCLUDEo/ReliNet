#pragma once

#include "protocol.hpp"
#include <QObject>
#include <QTimer>
#include <QMap>
#include <QSet>
#include <QDateTime>
#include <functional>
#include <memory>

namespace ReliNet {

struct InFlightMessage {
    Message message;
    QDateTime send_timestamp;
    int retry_count = 0;
    std::unique_ptr<QTimer> retry_timer;
    
    InFlightMessage() = default;
    InFlightMessage(const Message& msg);
    InFlightMessage(InFlightMessage&& other) noexcept;
    InFlightMessage& operator=(InFlightMessage&& other) noexcept;
    
    // Non-copyable due to unique_ptr
    InFlightMessage(const InFlightMessage&) = delete;
    InFlightMessage& operator=(const InFlightMessage&) = delete;
};

struct RetryConfig {
    int base_backoff_ms = 1000;        // Base backoff: 1 second
    int max_backoff_ms = 60000;        // Max backoff: 60 seconds
    int jitter_percent = 20;           // ±20% jitter
    
    // Priority-specific overrides
    int sos_max_backoff_ms = 5000;     // SOS/DISTRESS: max 5s backoff
    int urgent_max_backoff_ms = 15000; // URGENT: max 15s backoff
    
    int sos_max_retries = -1;          // Unlimited retries for SOS/DISTRESS
    int urgent_max_retries = 20;       // 20 retries for URGENT
    int routine_max_retries = 10;      // 10 retries for ROUTINE (configurable)
};

class RetryEngine : public QObject {
    Q_OBJECT
    
public:
    explicit RetryEngine(QObject* parent = nullptr);
    ~RetryEngine();
    
    void setConfig(const RetryConfig& config);
    const RetryConfig& getConfig() const { return config_; }
    
    // Send message with automatic retry
    void sendMessage(const Message& message);
    
    // Handle incoming ACK
    void handleAck(uint64_t message_id);
    
    // Handle incoming message (for deduplication)
    bool shouldProcessMessage(uint64_t message_id);
    
    // Force retry of specific message
    void forceRetry(uint64_t message_id);
    
    // Cancel message (for operator intervention)
    void cancelMessage(uint64_t message_id);
    
    // Statistics
    int getInFlightCount() const { return in_flight_messages_.size(); }
    QList<uint64_t> getInFlightMessageIds() const;
    
signals:
    void sendRequest(const QByteArray& data);        // Request transport to send data
    void deliveryConfirmed(uint64_t message_id, Priority priority);
    void deliveryFailed(uint64_t message_id, Priority priority);
    void retryAttempt(uint64_t message_id, int retry_count, Priority priority);
    void messageDeduplicationRequired(uint64_t message_id);
    
private slots:
    void onRetryTimer();
    
private:
    int calculateBackoffMs(Priority priority, int retry_count);
    int getMaxRetries(Priority priority);
    int getMaxBackoffMs(Priority priority);
    void scheduleRetry(uint64_t message_id);
    void removeInFlightMessage(uint64_t message_id);
    
    RetryConfig config_;
    QMap<uint64_t, std::unique_ptr<InFlightMessage>> in_flight_messages_;
    QSet<uint64_t> received_message_ids_; // For deduplication
    
    static constexpr int DEDUP_HISTORY_SIZE = 1000;
};

} // namespace ReliNet