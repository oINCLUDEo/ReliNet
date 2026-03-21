#include "outbox_queue.hpp"
#include <QDir>
#include <QDebug>
#include <algorithm>

namespace ReliNet {

// File format constants (used by OutboxRecord serialize/deserialize)
static constexpr uint32_t OUTBOX_MAGIC = 0xFEEDFACE;
static constexpr uint8_t  OUTBOX_VERSION = 1;

OutboxRecord::OutboxRecord(uint64_t id, Priority prio, ContextTag ctx, const QByteArray& data)
    : message_id(id), priority(prio), context_tag(ctx), status(MessageStatus::PENDING), payload(data), file_offset(-1) {
}

QByteArray OutboxRecord::serialize() const {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    
    // Record format: magic(4) + version(1) + message_id(8) + priority(1) + context(1) + status(1) + payload_len(4) + payload(var)
    stream << OUTBOX_MAGIC
           << OUTBOX_VERSION
           << static_cast<quint64>(message_id)
           << static_cast<uint8_t>(priority)
           << static_cast<uint8_t>(context_tag)
           << static_cast<uint8_t>(status)
           << static_cast<uint32_t>(payload.size());
    
    data.append(payload);
    return data;
}

Result<OutboxRecord> OutboxRecord::deserialize(const QByteArray& data, qint64 offset) {
    if (data.size() < 20) { // Minimum size without payload
        return Result<OutboxRecord>(ProtocolError::BufferTooSmall);
    }
    
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);
    
    uint32_t magic;
    uint8_t version, priority_raw, context_raw, status_raw;
    quint64 message_id_temp;
    uint32_t payload_len;
    
    stream >> magic >> version >> message_id_temp >> priority_raw >> context_raw >> status_raw >> payload_len;
    
    if (magic != OUTBOX_MAGIC) {
        return Result<OutboxRecord>(ProtocolError::InvalidMagic);
    }
    
    if (version != OUTBOX_VERSION) {
        return Result<OutboxRecord>(ProtocolError::InvalidVersion);
    }
    
    OutboxRecord record;
    record.message_id = message_id_temp;
    record.priority = static_cast<Priority>(priority_raw);
    record.context_tag = static_cast<ContextTag>(context_raw);
    record.status = static_cast<MessageStatus>(status_raw);
    record.file_offset = offset;
    
    if (static_cast<uint32_t>(data.size()) < 20 + payload_len) {
        return Result<OutboxRecord>(ProtocolError::BufferTooSmall);
    }
    
    record.payload = data.mid(20, payload_len);
    return Result<OutboxRecord>(record);
}

OutboxQueue::OutboxQueue(const QString& filename, QObject* parent)
    : QObject(parent), filename_(filename), stream_(&file_) {
    
    stream_.setByteOrder(QDataStream::BigEndian);
    
    if (!openFile()) {
        emit errorOccurred("Failed to open outbox file: " + filename_);
    }
    
    // Build message offset index
    auto records = loadAllRecords();
    for (const auto& record : records) {
        message_offsets_[record.message_id] = record.file_offset;
    }
}

OutboxQueue::~OutboxQueue() {
    flush();
}

bool OutboxQueue::enqueue(const Message& message) {
    QMutexLocker locker(&mutex_);
    
    OutboxRecord record(message.header.message_id, message.header.priority, 
                       message.header.context_tag, message.payload);
    
    if (!writeRecord(record)) {
        emit errorOccurred("Failed to write message to outbox");
        return false;
    }
    
    message_offsets_[message.header.message_id] = record.file_offset;
    emit messageEnqueued(message.header.message_id, message.header.priority);
    return true;
}

bool OutboxQueue::markAcked(uint64_t message_id) {
    QMutexLocker locker(&mutex_);
    
    auto it = message_offsets_.find(message_id);
    if (it == message_offsets_.end()) {
        return false; // Message not found
    }
    
    if (!updateRecordStatus(*it, MessageStatus::ACKED)) {
        emit errorOccurred("Failed to update message status to ACKED");
        return false;
    }
    
    emit messageAcked(message_id);
    return true;
}

bool OutboxQueue::markFailed(uint64_t message_id) {
    QMutexLocker locker(&mutex_);
    
    auto it = message_offsets_.find(message_id);
    if (it == message_offsets_.end()) {
        return false; // Message not found
    }
    
    if (!updateRecordStatus(*it, MessageStatus::FAILED)) {
        emit errorOccurred("Failed to update message status to FAILED");
        return false;
    }
    
    emit messageFailed(message_id);
    return true;
}

QList<OutboxRecord> OutboxQueue::recoverPendingMessages() {
    QMutexLocker locker(&mutex_);
    
    auto all_records = loadAllRecords();
    QList<OutboxRecord> pending;
    
    for (const auto& record : all_records) {
        if (record.status == MessageStatus::PENDING) {
            pending.append(record);
        }
    }
    
    // Sort by priority: SOS/DISTRESS first, then by priority value
    std::sort(pending.begin(), pending.end(), PriorityComparator());
    
    return pending;
}

QList<OutboxRecord> OutboxQueue::getPendingByPriority() {
    return recoverPendingMessages(); // Same implementation
}

int OutboxQueue::getPendingCount() const {
    QMutexLocker locker(&mutex_);
    
    auto all_records = loadAllRecords();
    return std::count_if(all_records.begin(), all_records.end(),
                        [](const OutboxRecord& r) { return r.status == MessageStatus::PENDING; });
}

int OutboxQueue::getTotalCount() const {
    QMutexLocker locker(&mutex_);
    return loadAllRecords().size();
}

qint64 OutboxQueue::getFileSize() const {
    return file_.size();
}

bool OutboxQueue::compactFile() {
    QMutexLocker locker(&mutex_);
    
    // Read all PENDING records
    auto pending = recoverPendingMessages();
    
    // Close current file
    file_.close();
    
    // Create backup
    QString backup_name = filename_ + ".backup";
    if (QFile::exists(backup_name)) {
        QFile::remove(backup_name);
    }
    QFile::rename(filename_, backup_name);
    
    // Recreate file with only pending records
    if (!openFile()) {
        // Restore backup on failure
        QFile::rename(backup_name, filename_);
        openFile();
        return false;
    }
    
    message_offsets_.clear();
    for (const auto& record : pending) {
        OutboxRecord new_record = record;
        new_record.file_offset = -1; // Will be set by writeRecord
        if (writeRecord(new_record)) {
            message_offsets_[new_record.message_id] = new_record.file_offset;
        }
    }
    
    // Remove backup on success
    QFile::remove(backup_name);
    return true;
}

void OutboxQueue::flush() {
    QMutexLocker locker(&mutex_);
    file_.flush();
}

bool OutboxQueue::openFile() {
    file_.setFileName(filename_);
    
    // Ensure directory exists
    QDir().mkpath(QFileInfo(filename_).dir().path());
    
    return file_.open(QIODevice::ReadWrite | QIODevice::Append);
}

bool OutboxQueue::writeRecord(const OutboxRecord& record) {
    if (!file_.isOpen()) {
        return false;
    }
    
    qint64 offset = file_.pos();
    QByteArray data = record.serialize();
    
    qint64 written = file_.write(data);
    if (written != data.size()) {
        return false;
    }
    
    // Update the record's offset (const_cast is safe here as we're just setting the offset)
    const_cast<OutboxRecord&>(record).file_offset = offset;
    
    file_.flush();
    return true;
}

bool OutboxQueue::updateRecordStatus(qint64 offset, MessageStatus status) {
    if (!file_.isOpen()) {
        return false;
    }
    
    qint64 current_pos = file_.pos();
    
    // Seek to status field (magic(4) + version(1) + message_id(8) + priority(1) + context(1) = 15 bytes offset)
    if (!file_.seek(offset + 15)) {
        return false;
    }
    
    uint8_t status_byte = static_cast<uint8_t>(status);
    qint64 written = file_.write(reinterpret_cast<const char*>(&status_byte), 1);
    
    // Restore position
    file_.seek(current_pos);
    file_.flush();
    
    return written == 1;
}

QList<OutboxRecord> OutboxQueue::loadAllRecords() const {
    QList<OutboxRecord> records;
    
    if (!file_.isOpen()) {
        return records;
    }
    
    qint64 current_pos = file_.pos();
    file_.seek(0);
    
    while (!file_.atEnd()) {
        qint64 record_offset = file_.pos();
        
        // Read magic and version first to determine record size
        uint32_t magic;
        uint8_t version;
        if (file_.read(reinterpret_cast<char*>(&magic), 4) != 4 ||
            file_.read(reinterpret_cast<char*>(&version), 1) != 1) {
            break;
        }
        
        if (magic != OUTBOX_MAGIC) {
            break; // Invalid record
        }
        
        // Read rest of header to get payload length
        file_.seek(record_offset + 16); // Skip to payload_len field
        uint32_t payload_len;
        if (file_.read(reinterpret_cast<char*>(&payload_len), 4) != 4) {
            break;
        }
        
        // Read entire record
        file_.seek(record_offset);
        QByteArray record_data = file_.read(20 + payload_len);
        
        if (record_data.size() == 20 + static_cast<int>(payload_len)) {
            auto record = OutboxRecord::deserialize(record_data, record_offset);
            if (record) {
                records.append(*record);
            }
        }
    }
    
    file_.seek(current_pos);
    return records;
}

} // namespace ReliNet

#include "outbox_queue.moc"