#pragma once

#include "protocol.hpp"
#include <QObject>
#include <QFile>
#include <QDataStream>
#include <QMutex>
#include <QMap>

namespace ReliNet {

enum class MessageStatus : uint8_t {
    PENDING = 0,
    ACKED = 1,
    FAILED = 2
};

struct OutboxRecord {
    uint64_t message_id;
    Priority priority;
    ContextTag context_tag;
    MessageStatus status;
    QByteArray payload;
    qint64 file_offset; // Position in file for in-place updates
    
    OutboxRecord() = default;
    OutboxRecord(uint64_t id, Priority prio, ContextTag ctx, const QByteArray& data);
    
    QByteArray serialize() const;
    static Result<OutboxRecord> deserialize(const QByteArray& data, qint64 offset);
    
    bool isEmergency() const { return ProtocolCodec::isPriorityEmergency(priority); }
};

class OutboxQueue : public QObject {
    Q_OBJECT
    
private:
    static constexpr uint32_t OUTBOX_MAGIC = 0xFEEDFACE;
    static constexpr uint8_t OUTBOX_VERSION = 1;
    
public:
    explicit OutboxQueue(const QString& filename = "outbox.bin", QObject* parent = nullptr);
    ~OutboxQueue();
    
    // Core operations
    bool enqueue(const Message& message);
    bool markAcked(uint64_t message_id);
    bool markFailed(uint64_t message_id);
    
    // Recovery operations
    QList<OutboxRecord> recoverPendingMessages();
    QList<OutboxRecord> getPendingByPriority();
    
    // Statistics
    int getPendingCount() const;
    int getTotalCount() const;
    qint64 getFileSize() const;
    
    // Maintenance
    bool compactFile(); // Remove ACKED/FAILED records, keep only PENDING
    void flush(); // Force write to disk
    
signals:
    void messageEnqueued(uint64_t message_id, Priority priority);
    void messageAcked(uint64_t message_id);
    void messageFailed(uint64_t message_id);
    void errorOccurred(const QString& error);
    
private:
    bool openFile();
    bool writeRecord(const OutboxRecord& record);
    bool updateRecordStatus(qint64 offset, MessageStatus status);
    QList<OutboxRecord> loadAllRecords();
    
    QString filename_;
    QFile file_;
    QDataStream stream_;
    mutable QMutex mutex_; // Thread-safe access
    QMap<uint64_t, qint64> message_offsets_; // message_id -> file offset for fast status updates
    
    // static constexpr uint32_t OUTBOX_MAGIC = 0xFEEDFACE;
    // static constexpr uint8_t OUTBOX_VERSION = 1;
};

// Priority comparator for sorting
struct PriorityComparator {
    bool operator()(const OutboxRecord& a, const OutboxRecord& b) const {
        // SOS/DISTRESS first, then by priority value (higher = more urgent)
        if (a.isEmergency() && !b.isEmergency()) return true;
        if (!a.isEmergency() && b.isEmergency()) return false;
        
        return static_cast<uint8_t>(a.priority) > static_cast<uint8_t>(b.priority);
    }
};

} // namespace ReliNet