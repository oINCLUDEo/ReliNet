#include "protocol.hpp"
#include <QDataStream>
#include <QCryptographicHash>
#include <QDebug>

namespace ReliNet {

Message::Message(uint64_t id, Priority prio, ContextTag ctx, const QByteArray& data) 
    : payload(data) 
{
    header.message_id = id;
    header.priority = prio;
    header.context_tag = ctx;
    header.payload_length = static_cast<uint32_t>(data.size());
    checksum = calculateChecksum();
}

uint32_t Message::calculateChecksum() const {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    
    stream << header.magic << header.version << static_cast<quint64>(header.message_id) 
           << static_cast<uint8_t>(header.priority)
           << static_cast<uint8_t>(header.context_tag)
           << header.payload_length;
    data.append(payload);
    
    return ProtocolCodec::calculateCRC32(data);
}

bool Message::validateChecksum() const {
    return checksum == calculateChecksum();
}

Result<QByteArray> ProtocolCodec::serialize(const Message& message) {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    
    // Write header
    stream << message.header.magic 
           << message.header.version
           << static_cast<quint64>(message.header.message_id)
           << static_cast<uint8_t>(message.header.priority)
           << static_cast<uint8_t>(message.header.context_tag)
           << message.header.payload_length;
    
    // Write payload
    data.append(message.payload);
    
    // Calculate and write checksum
    uint32_t checksum = calculateCRC32(data);
    stream << checksum;
    
    return Result<QByteArray>(data);
}

Result<Message> ProtocolCodec::deserialize(const QByteArray& data) {
    if (data.size() < 20) { // Minimum size: header (16) + checksum (4)
        return Result<Message>(ProtocolError::BufferTooSmall);
    }
    
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);
    
    Message message;
    uint8_t priority_raw, context_raw;
    quint64 message_id;
    
    // Read header
    stream >> message.header.magic 
           >> message.header.version
           >> message_id
           >> priority_raw
           >> context_raw
           >> message.header.payload_length;
           
    message.header.message_id = message_id;
    
    if (message.header.magic != PROTOCOL_MAGIC) {
        return Result<Message>(ProtocolError::InvalidMagic);
    }
    
    if (message.header.version != PROTOCOL_VERSION) {
        return Result<Message>(ProtocolError::InvalidVersion);
    }
    
    message.header.priority = static_cast<Priority>(priority_raw);
    message.header.context_tag = static_cast<ContextTag>(context_raw);
    
    // Check if we have enough data for payload + checksum
    if (data.size() < 16 + message.header.payload_length + 4) {
        return Result<Message>(ProtocolError::BufferTooSmall);
    }
    
    // Read payload
    message.payload = data.mid(16, message.header.payload_length);
    
    // Read checksum
    stream.device()->seek(16 + message.header.payload_length);
    stream >> message.checksum;
    
    // Validate checksum
    if (!message.validateChecksum()) {
        return Result<Message>(ProtocolError::InvalidChecksum);
    }
    
    return Result<Message>(message);
}

Result<QByteArray> ProtocolCodec::extractFrame(const QByteArray& buffer, int& bytesConsumed) {
    bytesConsumed = 0;
    
    if (buffer.size() < 16) {
        return Result<QByteArray>(ProtocolError::BufferTooSmall);
    }
    
    // Find magic bytes
    int magicPos = buffer.indexOf(QByteArray(reinterpret_cast<const char*>(&PROTOCOL_MAGIC), 2));
    if (magicPos == -1) {
        bytesConsumed = buffer.size();
        return Result<QByteArray>(ProtocolError::InvalidMagic);
    }
    
    if (magicPos > 0) {
        bytesConsumed = magicPos;
        return Result<QByteArray>(ProtocolError::InvalidMagic); // Skip invalid data
    }
    
    // Read payload length
    if (buffer.size() < magicPos + 12) {
        return Result<QByteArray>(ProtocolError::BufferTooSmall);
    }
    
    QDataStream stream(buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.device()->seek(magicPos + 8); // Skip to payload_length field
    
    uint32_t payload_length;
    stream >> payload_length;
    
    int total_frame_size = 16 + payload_length + 4; // header + payload + checksum
    
    if (buffer.size() < magicPos + total_frame_size) {
        return Result<QByteArray>(ProtocolError::BufferTooSmall);
    }
    
    bytesConsumed = magicPos + total_frame_size;
    return Result<QByteArray>(buffer.mid(magicPos, total_frame_size));
}

uint32_t ProtocolCodec::calculateCRC32(const QByteArray& data) {
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(data);
    QByteArray result = hash.result();
    
    // Use first 4 bytes of MD5 as simple checksum
    // In production, use proper CRC32 implementation
    uint32_t crc = 0;
    for (int i = 0; i < 4 && i < result.size(); ++i) {
        crc = (crc << 8) | static_cast<uint8_t>(result[i]);
    }
    return crc;
}

bool ProtocolCodec::isPriorityEmergency(Priority priority) {
    return priority == Priority::DISTRESS || priority == Priority::SOS;
}

QString ProtocolCodec::priorityToString(Priority priority) {
    switch (priority) {
    case Priority::ROUTINE: return "ROUTINE";
    case Priority::URGENT: return "URGENT";  
    case Priority::DISTRESS: return "DISTRESS";
    case Priority::SOS: return "SOS";
    default: return QString("UNKNOWN(%1)").arg(static_cast<int>(priority));
    }
}

QString ProtocolCodec::contextTagToString(ContextTag context) {
    switch (context) {
    case ContextTag::MARITIME: return "MARITIME";
    case ContextTag::MILITARY: return "MILITARY";
    case ContextTag::GENERAL: return "GENERAL";
    default: return QString("UNKNOWN(%1)").arg(static_cast<int>(context));
    }
}

// ACK Message Implementation
QByteArray AckMessage::serialize() const {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint64>(original_message_id);
    return data;
}

Result<AckMessage> AckMessage::deserialize(const QByteArray& data) {
    if (data.size() < 8) {
        return Result<AckMessage>(ProtocolError::BufferTooSmall);
    }
    
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);
    
    AckMessage ack;
    quint64 msg_id;
    stream >> msg_id;
    ack.original_message_id = msg_id;
    return Result<AckMessage>(ack);
}

// Heartbeat Message Implementation  
QByteArray HeartbeatMessage::serialize() const {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << type << static_cast<quint64>(timestamp);
    return data;
}

Result<HeartbeatMessage> HeartbeatMessage::deserialize(const QByteArray& data) {
    if (data.size() < 9) {
        return Result<HeartbeatMessage>(ProtocolError::BufferTooSmall);
    }
    
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);
    
    HeartbeatMessage hb;
    quint64 ts;
    stream >> hb.type >> ts;
    hb.timestamp = ts;
    return Result<HeartbeatMessage>(hb);
}

} // namespace ReliNet