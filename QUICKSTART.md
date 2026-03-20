# ReliNet Quick Start Guide

## 5-Minute Integration

### 1. Link the Library

**CMakeLists.txt:**
```cmake
add_subdirectory(path/to/ReliNet)
target_link_libraries(YourApp PRIVATE ReliNetLib)
```

### 2. Include Headers

```cpp
#include <ReliNet/MessageManager.h>
#include <ReliNet/PersistentQueue.h>
#include <ReliNet/DeliveryEngine.h>
#include <ReliNet/ChannelManager.h>
#include <ReliNet/PrimaryChannel.h>
#include <ReliNet/BackupChannel.h>
```

### 3. Initialize (One-Time Setup)

```cpp
using namespace ReliNet;

// Create queue
PersistentQueue* queue = new PersistentQueue("app_messages.db");
queue->initialize();

// Setup channels
ChannelManager* channelMgr = new ChannelManager();

auto* primary = new PrimaryChannel("server.example.com", 12345);
auto* backup = new BackupChannel("backup.example.com", 12346);

channelMgr->addChannel(primary);
channelMgr->addChannel(backup);

primary->connectToServer();
backup->connectToServer();

// Create delivery engine
DeliveryEngine* engine = new DeliveryEngine(queue, channelMgr);
engine->setAckTimeout(5000);    // 5 seconds
engine->setMaxRetries(5);
engine->start();

// Create message manager
MessageManager* msgMgr = new MessageManager(queue);
```

### 4. Send Messages

```cpp
// Simple message
msgMgr->sendMessage("Hello, World!", Priority::NORMAL);

// High priority message
msgMgr->sendMessage("Important update", Priority::HIGH);

// Critical message (sent via all channels)
msgMgr->sendMessage("System alert", Priority::CRITICAL);

// With JSON payload
QJsonObject data;
data["user_id"] = 12345;
data["action"] = "login";
QJsonDocument doc(data);
msgMgr->sendMessage(doc.toJson(), Priority::NORMAL);
```

### 5. Monitor Delivery (Optional)

```cpp
// Connect to signals
QObject::connect(engine, &DeliveryEngine::messageDelivered,
    [](const QString& id) {
        qDebug() << "✓ Delivered:" << id;
    });

QObject::connect(engine, &DeliveryEngine::messageFailed,
    [](const QString& id, const QString& reason) {
        qDebug() << "✗ Failed:" << id << reason;
    });

QObject::connect(engine, &DeliveryEngine::retryingMessage,
    [](const QString& id, int retry) {
        qDebug() << "↻ Retry" << retry << ":" << id;
    });
```

## Common Use Cases

### Use Case 1: Send and Forget

```cpp
MessageManager msgMgr(queue);
msgMgr.sendMessage("Event logged", Priority::LOW);
// Message will be delivered automatically with retries
```

### Use Case 2: Critical Notification

```cpp
// Sent via all available channels for redundancy
msgMgr.sendMessage("Server going down", Priority::CRITICAL);
```

### Use Case 3: Batch Messages

```cpp
QStringList events = {"Event 1", "Event 2", "Event 3"};
for (const QString& event : events) {
    msgMgr.sendMessage(event.toUtf8(), Priority::NORMAL);
}
// All messages queued and will be delivered in order by priority
```

### Use Case 4: Custom Priority Logic

```cpp
Priority getPriority(int errorCode) {
    if (errorCode >= 500) return Priority::CRITICAL;
    if (errorCode >= 400) return Priority::HIGH;
    return Priority::NORMAL;
}

msgMgr.sendMessage(errorData, getPriority(errorCode));
```

### Use Case 5: Handling Connection Loss

```cpp
// Library handles this automatically!
// Messages are queued persistently
// Delivery resumes when connection is restored
// No action needed from your application
```

## Server Implementation

### Minimal Server

```cpp
class MyServer : public QObject {
    Q_OBJECT
public:
    MyServer(quint16 port) {
        server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, this, &MyServer::onConnection);
        server->listen(QHostAddress::Any, port);
    }

private slots:
    void onConnection() {
        QTcpSocket* socket = server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, [this, socket]() {
            handleMessage(socket);
        });
    }

    void handleMessage(QTcpSocket* socket) {
        QByteArray data = socket->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject msg = doc.object();

        QString msgId = msg["id"].toString();

        // Process message here
        processYourData(msg["payload"].toString());

        // Send ACK
        QJsonObject ack;
        ack["message_id"] = msgId;
        ack["status"] = "ACK";
        socket->write(QJsonDocument(ack).toJson(QJsonDocument::Compact));
        socket->write("\n");
        socket->flush();
    }

private:
    QTcpServer* server;
};
```

## Configuration Tips

### Tuning for Your Use Case

**High-Volume, Low-Latency:**
```cpp
engine->setProcessInterval(100);  // Check queue every 100ms
engine->setAckTimeout(1000);      // 1 second timeout
engine->setMaxRetries(3);         // Fast failure
```

**Low-Volume, High-Reliability:**
```cpp
engine->setProcessInterval(5000); // Check every 5 seconds
engine->setAckTimeout(30000);     // 30 second timeout
engine->setMaxRetries(10);        // Many retries
```

**Unreliable Network:**
```cpp
engine->setAckTimeout(10000);     // 10 second timeout
engine->setMaxRetries(20);        // Very persistent
// Exponential backoff handles this automatically
```

## Debugging

### Enable Detailed Logging

```cpp
// Qt handles qDebug output
// Set environment variable for more details:
// QT_LOGGING_RULES="*.debug=true"

// Or in code:
QLoggingCategory::setFilterRules("*.debug=true");
```

### Check Database Contents

```bash
sqlite3 app_messages.db
sqlite> SELECT * FROM messages;
sqlite> SELECT * FROM messages WHERE status = 'PENDING';
sqlite> SELECT COUNT(*) FROM messages WHERE status = 'ACKED';
```

### Monitor Network Traffic

```bash
# Linux/Mac
sudo tcpdump -i lo port 12345 -A

# Windows
# Use Wireshark with filter: tcp.port == 12345
```

## Troubleshooting

### Messages Not Being Sent

1. Check channels are connected:
```cpp
qDebug() << "Primary:" << primary->isAvailable();
qDebug() << "Backup:" << backup->isAvailable();
```

2. Verify engine is running:
```cpp
// Make sure you called:
engine->start();
```

3. Check queue:
```cpp
QList<Message> pending = queue->getPendingMessages();
qDebug() << "Pending messages:" << pending.size();
```

### Messages Not Being Acknowledged

1. Verify server is sending ACK
2. Check ACK format is correct (JSON with `message_id` and `status: "ACK"`)
3. Ensure newline delimiter is present

### High Retry Count

1. Check network connectivity
2. Verify server is running and accessible
3. Increase ACK timeout if network is slow
4. Check server logs for errors

## Performance Tips

1. **Reuse Components**: Create MessageManager, DeliveryEngine once, reuse throughout app
2. **Batch Operations**: Queue multiple messages before starting engine
3. **Database**: Use SSD for better SQLite performance
4. **Priorities**: Use wisely - CRITICAL sends to all channels (bandwidth intensive)
5. **Cleanup**: Periodically delete old ACKED messages from database

## Memory Management

Using Qt parent-child ownership:

```cpp
// Option 1: Let Qt manage memory
queue->setParent(app);
engine->setParent(app);

// Option 2: Manual cleanup
delete engine;
delete channelMgr;  // Deletes channels too
delete queue;

// Option 3: Smart pointers
auto queue = std::make_unique<PersistentQueue>("db.db");
```

## Next Steps

1. Read [ARCHITECTURE.md](ARCHITECTURE.md) for deep dive
2. Check [BUILD.md](BUILD.md) for build instructions
3. Review [client/client_main.cpp](client/client_main.cpp) for complete example
4. Modify priorities and timeouts for your needs
5. Implement your custom channels if needed

## Support

For issues or questions:
- Check the source code comments
- Review the example applications
- See ARCHITECTURE.md for detailed design
- Test with the provided server/client applications first
