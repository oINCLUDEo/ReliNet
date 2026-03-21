#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QString>
#include <cstdint>
#include <optional>

namespace ReliNet {

// Magic bytes for protocol identification
constexpr uint16_t PROTOCOL_MAGIC = 0xDEAD;
constexpr uint8_t PROTOCOL_VERSION = 1;

enum class Priority : uint8_t {
    ROUTINE = 0,
    URGENT = 64,
    DISTRESS = 128,
    SOS = 255
};

enum class ContextTag : uint8_t {
    MARITIME = 0x01,
    MILITARY = 0x02,
    GENERAL = 0x03
};

enum class ProtocolError {
    InvalidMagic,
    InvalidVersion,
    InvalidLength,
    InvalidChecksum,
    BufferTooSmall,
    SerializationError
};

// Simple result wrapper for C++20 compatibility
template<typename T>
class Result {
public:
    Result(const T& value) : value_(value), has_value_(true) {}
    Result(ProtocolError error) : error_(error), has_value_(false) {}
    
    bool has_value() const { return has_value_; }
    const T& operator*() const { return value_; }
    const T* operator->() const { return &value_; }
    ProtocolError error() const { return error_; }
    
    explicit operator bool() const { return has_value_; }
    
private:
    T value_{};
    ProtocolError error_ = ProtocolError::SerializationError;
    bool has_value_ = false;
};

struct MessageHeader {
    uint16_t magic = PROTOCOL_MAGIC;
    uint8_t version = PROTOCOL_VERSION;
    uint64_t message_id = 0;
    Priority priority = Priority::ROUTINE;
    ContextTag context_tag = ContextTag::GENERAL;
    uint32_t payload_length = 0;
};

struct Message {
    MessageHeader header;
    QByteArray payload;
    uint32_t checksum = 0;
    
    Message() = default;
    Message(uint64_t id, Priority prio, ContextTag ctx, const QByteArray& data);
    
    // Calculate CRC32 checksum
    uint32_t calculateChecksum() const;
    bool validateChecksum() const;
};

class ProtocolCodec {
public:
    static Result<QByteArray> serialize(const Message& message);
    static Result<Message> deserialize(const QByteArray& data);
    
    // Frame detection for stream-based transports
    static Result<QByteArray> extractFrame(const QByteArray& buffer, int& bytesConsumed);
    
    // Utility functions
    static bool isPriorityEmergency(Priority priority);
    static QString priorityToString(Priority priority);
    static QString contextTagToString(ContextTag context);
    static uint32_t calculateCRC32(const QByteArray& data);
    
private:
    // No private members needed
};

// ACK message helpers
struct AckMessage {
    uint64_t original_message_id;
    
    QByteArray serialize() const;
    static Result<AckMessage> deserialize(const QByteArray& data);
};

// Heartbeat message helpers  
struct HeartbeatMessage {
    enum Type : uint8_t { PING = 1, PONG = 2 };
    Type type;
    uint64_t timestamp;
    
    QByteArray serialize() const;
    static Result<HeartbeatMessage> deserialize(const QByteArray& data);
};

} // namespace ReliNet