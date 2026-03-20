# ReliNet Architecture Documentation

## System Overview

ReliNet is designed as a **reusable library** for guaranteed message delivery. The architecture follows clean separation of concerns with well-defined interfaces.

## Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        CLIENT APPLICATION                        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      MessageManager                              │
│  • Creates messages with UUID                                    │
│  • Sets priority                                                 │
│  • Queues messages                                              │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    PersistentQueue (SQLite)                      │
│  • Stores messages on disk                                       │
│  • Manages status (PENDING/SENT/ACKED)                          │
│  • Tracks retry count                                           │
│  • Priority-based retrieval                                     │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      DeliveryEngine                              │
│  • Processes message queue                                       │
│  • Selects appropriate channel                                   │
│  • Manages retry logic                                          │
│  • Handles ACK timeouts                                         │
│  • Exponential backoff                                          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      ChannelManager                              │
│  • Manages multiple channels                                     │
│  • Provides primary/backup channels                             │
│  • Tracks channel availability                                  │
└─────────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   Primary    │     │    Backup    │     │  Emergency   │
│   Channel    │     │   Channel    │     │   Channel    │
│   (TCP)      │     │   (TCP)      │     │  (Future)    │
└──────────────┘     └──────────────┘     └──────────────┘
        │                     │                     │
        └─────────────────────┼─────────────────────┘
                              │
                              ▼
                        Network (TCP)
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      SERVER APPLICATION                          │
│  • Receives messages                                             │
│  • Detects duplicates                                           │
│  • Sends ACK                                                    │
└─────────────────────────────────────────────────────────────────┘
```

## Class Hierarchy

```
QObject
    │
    ├── IChannel (abstract)
    │       ├── PrimaryChannel
    │       └── BackupChannel
    │
    ├── MessageManager
    │
    ├── PersistentQueue
    │
    ├── DeliveryEngine
    │
    └── ChannelManager
```

## Data Flow

### 1. Message Creation and Queueing

```
User Code
    │
    ▼
MessageManager::sendMessage(data, priority)
    │
    ├── Generate UUID
    ├── Create Message object
    │
    ▼
PersistentQueue::addMessage(message)
    │
    ├── Insert into SQLite
    ├── Status = PENDING
    └── Emit signal: messageAdded
```

### 2. Message Delivery

```
DeliveryEngine::processQueue() [Timer-based]
    │
    ▼
PersistentQueue::getPendingMessages()
    │
    ▼
DeliveryEngine::selectChannel(message, retryCount)
    │
    ├── If LOW/NORMAL → Primary (or Backup if unavailable)
    ├── If HIGH → Primary on first try, Backup on retry
    └── If CRITICAL → ALL available channels
    │
    ▼
IChannel::send(message)
    │
    ├── Serialize to JSON
    ├── Send via QTcpSocket
    └── Emit signal: messageSent
    │
    ▼
DeliveryEngine::setupAckTimer(messageId)
    │
    ├── Calculate timeout with backoff
    └── Start QTimer
```

### 3. ACK Reception

```
Server receives message
    │
    ├── Check for duplicate (messageId)
    │
    ▼
Send ACK { message_id, status: "ACK" }
    │
    ▼
Channel::onReadyRead()
    │
    ├── Parse JSON
    │
    ▼
Channel::ackReceived(messageId) [signal]
    │
    ▼
DeliveryEngine::onAckReceived(messageId)
    │
    ├── Stop ACK timer
    ├── Remove from pending
    │
    ▼
PersistentQueue::markAsAcked(messageId)
    │
    └── Update status in SQLite
```

### 4. Timeout and Retry

```
ACK Timer expires
    │
    ▼
DeliveryEngine::handleAckTimeout(messageId)
    │
    ├── Remove from pending
    ├── Check retry count < max
    │
    ▼
PersistentQueue::incrementRetry(messageId)
    │
    ├── retry_count++
    ├── status = PENDING
    │
    ▼
Next processQueue() cycle
    │
    └── Retry with exponential backoff
```

## Message States

```
     ┌─────────┐
     │ PENDING │ ◄──────────────┐
     └────┬────┘                │
          │                     │
          │ Send               │ Retry
          │                     │
          ▼                     │
     ┌────────┐    Timeout     │
     │  SENT  │ ───────────────┘
     └────┬───┘
          │
          │ ACK received
          │
          ▼
     ┌────────┐
     │ ACKED  │
     └────────┘
```

## Priority Routing Strategy

### LOW Priority
```
Try Primary → If unavailable, try Backup → If all fail, retry later
```

### NORMAL Priority
```
Try Primary → If unavailable, try Backup → If all fail, retry later
```

### HIGH Priority
```
Attempt 0: Primary
Attempt 1+: Backup (fallback after first failure)
```

### CRITICAL Priority
```
Send via ALL available channels simultaneously
First ACK wins, others ignored
```

## Threading Model

ReliNet uses Qt's event-driven architecture:

- **Main Thread**: All components run in Qt event loop
- **QTimer**: Used for periodic queue processing and ACK timeouts
- **Signals/Slots**: Asynchronous communication between components
- **QTcpSocket**: Non-blocking I/O with event notifications

**Benefits:**
- No manual thread management
- Thread-safe through Qt's signal/slot mechanism
- Efficient event-driven processing
- Easy to integrate into Qt applications

## Database Schema

### messages table

```sql
CREATE TABLE messages (
    id TEXT PRIMARY KEY,           -- UUID
    payload TEXT NOT NULL,         -- Message content
    priority TEXT NOT NULL,        -- LOW/NORMAL/HIGH/CRITICAL
    status TEXT NOT NULL,          -- PENDING/SENT/ACKED
    retry_count INTEGER DEFAULT 0, -- Number of retry attempts
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

## Network Protocol

### Message Format (Client → Server)

```json
{
  "id": "550e8400-e29b-41d4-a716-446655440000",
  "payload": "Hello, World!",
  "priority": "HIGH"
}
```

### ACK Format (Server → Client)

```json
{
  "message_id": "550e8400-e29b-41d4-a716-446655440000",
  "status": "ACK"
}
```

**Protocol Details:**
- Transport: TCP
- Encoding: UTF-8
- Format: JSON
- Delimiter: Newline (`\n`)
- Each message is one line (newline-delimited)

## Configuration Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| ACK Timeout | 5000 ms | Time to wait for ACK before retry |
| Max Retries | 5 | Maximum retry attempts |
| Process Interval | 1000 ms | Queue check frequency |
| Primary Port | 12345 | Primary server port |
| Backup Port | 12346 | Backup server port |

## Extensibility Points

### 1. Adding New Channel Types

Implement `IChannel` interface:

```cpp
class SatelliteChannel : public IChannel {
public:
    void send(const Message& msg) override;
    bool isAvailable() const override;
    QString name() const override;
};
```

### 2. Custom Delivery Strategies

Modify `DeliveryEngine::selectChannel()`:

```cpp
IChannel* DeliveryEngine::selectChannel(const Message& msg, int retryCount) {
    // Custom logic here
}
```

### 3. Alternative Storage Backend

Implement queue interface with different backend:

```cpp
class RedisQueue : public QObject {
    // Implement same methods as PersistentQueue
};
```

### 4. Message Encryption

Add encryption layer in channels:

```cpp
void SecureChannel::send(const Message& msg) override {
    QByteArray encrypted = encrypt(msg.serialize());
    socket->write(encrypted);
}
```

## Error Handling

### Network Errors
- **Connection Lost**: Automatic reconnection via Qt signals
- **Send Failure**: Message stays in PENDING state for retry
- **Timeout**: Exponential backoff retry

### Database Errors
- **Write Failure**: Error signal emitted, message not queued
- **Read Failure**: Empty list returned, logged

### Message Errors
- **Invalid JSON**: Ignored, logged
- **Duplicate**: Detected by server, ACK sent anyway
- **Max Retries**: Message removed from queue, failure signal emitted

## Performance Considerations

### Database
- SQLite provides adequate performance for message queue
- Index on `status` column for fast pending message retrieval
- Transactions used for consistency

### Network
- Non-blocking I/O prevents thread blocking
- Buffering handles partial reads
- Newline delimiters enable simple framing

### Memory
- Messages loaded on-demand from database
- Pending messages kept in memory during delivery
- Timers cleaned up immediately after use

## Security Considerations

### Current Implementation
- ⚠️ No encryption (plain TCP)
- ⚠️ No authentication
- ⚠️ No message integrity verification

### Recommendations for Production
1. **Add TLS**: Use QSslSocket instead of QTcpSocket
2. **Authenticate**: Implement client/server authentication
3. **Sign Messages**: Add HMAC or digital signatures
4. **Validate Input**: Sanitize all received data
5. **Rate Limiting**: Prevent DoS attacks

## Testing Strategy

### Unit Tests (Future)
- Message serialization/deserialization
- Queue operations (add, retrieve, update)
- Channel selection logic
- Retry and backoff calculations

### Integration Tests (Future)
- Full client-server communication
- Network failure scenarios
- Multi-channel delivery
- Priority handling

### Manual Testing
1. Normal operation with stable network
2. Server disconnection/reconnection
3. Multiple priority messages
4. Queue persistence across restarts
5. Concurrent messages

## Deployment

### Library Deployment
1. Build static or shared library
2. Install headers to include directory
3. Link against Qt6::Core, Qt6::Network, Qt6::Sql

### Application Deployment
1. Bundle Qt libraries with application
2. Include SQLite plugin (qsqlite.dll/so)
3. Set library paths appropriately

### Production Considerations
- Log rotation for application logs
- Database backup strategy
- Monitoring and metrics
- Graceful shutdown handling
- Resource limits (max queue size)
