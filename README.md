# ReliNet - Reliable Message Delivery System

A robust, production-ready C++ library for guaranteed message delivery in unstable network conditions, built with Qt 6.

## 🎯 Overview

ReliNet is a reusable component that provides guaranteed message delivery between client and server with support for:

- ✅ Persistent message queue with SQLite storage
- ✅ Automatic retry mechanism with exponential backoff
- ✅ Delivery acknowledgments (ACK)
- ✅ Message prioritization (LOW, NORMAL, HIGH, CRITICAL)
- ✅ Multi-channel delivery with automatic fallback
- ✅ Network resilience and reconnection handling

## 🏗️ Architecture

### Core Components

#### 1. **Message**
- Represents a message with unique ID (UUID), payload, and priority
- JSON serialization/deserialization
- Status tracking (PENDING, SENT, ACKED)

#### 2. **MessageManager**
- Creates messages with unique IDs
- Queues messages for delivery
- Simple API: `sendMessage(data, priority)`

#### 3. **PersistentQueue**
- SQLite-based storage for reliability
- Persists messages across application restarts
- Priority-based retrieval
- Status management and retry counting

#### 4. **DeliveryEngine**
- Core delivery logic with retry mechanism
- ACK timeout handling with exponential backoff
- Channel selection based on priority and retry count
- Automatic retry on failure

#### 5. **Channel System**
- **IChannel**: Abstract interface for communication channels
- **PrimaryChannel**: TCP-based primary communication
- **BackupChannel**: TCP-based fallback communication
- **ChannelManager**: Coordinates multiple channels

## 📊 Delivery Strategies

### Priority-Based Routing

| Priority | Strategy |
|----------|----------|
| LOW/NORMAL | Use primary channel; fallback to backup if unavailable |
| HIGH | Use primary first; switch to backup on first retry |
| CRITICAL | Send via ALL available channels simultaneously |

### Retry Mechanism

- Automatic retry on timeout or failure
- Exponential backoff: 1s, 2s, 4s, 8s, ...
- Configurable max retries (default: 5)
- Configurable ACK timeout (default: 5s)

## 🔧 Building the Project

### Requirements

- CMake 3.16+
- Qt 6 (Core, Network, Sql modules)
- C++17 compiler

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

This will create:
- `libReliNetLib.a` - The reusable library
- `ReliNetServer` - Server application
- `ReliNetClient` - Client demo application

## 🚀 Usage

### Running the Server

```bash
./ReliNetServer
```

The server listens on two ports:
- Primary: 12345
- Backup: 12346

### Running the Client

```bash
./ReliNetClient
```

The client will:
1. Connect to both primary and backup servers
2. Send test messages with different priorities
3. Handle retries and fallbacks automatically
4. Display delivery status in real-time

### Using the Library in Your Code

```cpp
#include <ReliNet/MessageManager.h>
#include <ReliNet/PersistentQueue.h>
#include <ReliNet/DeliveryEngine.h>
#include <ReliNet/ChannelManager.h>
#include <ReliNet/PrimaryChannel.h>

using namespace ReliNet;

// Initialize queue
PersistentQueue queue("myapp.db");
queue.initialize();

// Setup channels
ChannelManager channelManager;
auto* primary = new PrimaryChannel("server.com", 12345);
channelManager.addChannel(primary);
primary->connectToServer();

// Setup delivery
DeliveryEngine engine(&queue, &channelManager);
engine.start();

// Send messages
MessageManager msgManager(&queue);
msgManager.sendMessage("Hello, World!", Priority::HIGH);
```

## 📁 Project Structure

```
ReliNet/
├── lib/
│   ├── include/              # Public headers
│   │   ├── Message.h
│   │   ├── MessageManager.h
│   │   ├── PersistentQueue.h
│   │   ├── DeliveryEngine.h
│   │   ├── ChannelManager.h
│   │   ├── IChannel.h
│   │   ├── PrimaryChannel.h
│   │   └── BackupChannel.h
│   └── src/                  # Implementation files
├── server/
│   └── server_main.cpp       # Server application
├── client/
│   └── client_main.cpp       # Client demo
└── CMakeLists.txt
```

## 🔌 Protocol

### Message Format (JSON)

```json
{
  "id": "uuid-string",
  "payload": "message data",
  "priority": "HIGH"
}
```

### ACK Format (JSON)

```json
{
  "message_id": "uuid-string",
  "status": "ACK"
}
```

Messages are newline-delimited over TCP.

## 🎨 Features

### ✅ Implemented

- [x] Message creation with UUID
- [x] Priority levels (LOW, NORMAL, HIGH, CRITICAL)
- [x] SQLite persistent queue
- [x] TCP-based primary and backup channels
- [x] Automatic retry with exponential backoff
- [x] ACK-based delivery confirmation
- [x] Multi-channel delivery for CRITICAL messages
- [x] Channel fallback on failure
- [x] Duplicate message detection on server
- [x] Clean separation of concerns

### 🚧 Future Enhancements

- [ ] UDP channel support
- [ ] Network condition emulator
- [ ] Message compression
- [ ] End-to-end encryption
- [ ] Delivery metrics and monitoring
- [ ] GUI demo application
- [ ] Unit tests

## 🔒 Reliability Features

1. **Persistent Storage**: Messages survive application crashes
2. **Retry Logic**: Automatic retries with backoff
3. **Multi-Channel**: Redundant delivery paths
4. **ACK Confirmation**: Guaranteed delivery verification
5. **Duplicate Detection**: Server-side deduplication
6. **Priority Handling**: Critical messages get special treatment

## 📝 License

This is a demonstration project for educational purposes.

## 🤝 Contributing

This project demonstrates clean architecture and extensibility:
- Easy to add new channel types
- Pluggable delivery strategies
- Testable components with clear interfaces

---

**Built with Qt 6 and modern C++17**
