# ReliNet Project Structure

```
ReliNet/
│
├── README.md                    # Main documentation
├── QUICKSTART.md               # Quick integration guide
├── BUILD.md                    # Build instructions
├── ARCHITECTURE.md             # Detailed architecture documentation
├── CMakeLists.txt              # Main build configuration
├── .gitignore                  # Git ignore rules
│
├── lib/                        # ReliNet Library (Reusable Component)
│   ├── include/                # Public API Headers
│   │   ├── Message.h          # Message class with priority and serialization
│   │   ├── MessageManager.h   # High-level API for sending messages
│   │   ├── PersistentQueue.h  # SQLite-based message queue
│   │   ├── DeliveryEngine.h   # Core delivery logic with retry
│   │   ├── ChannelManager.h   # Multi-channel coordinator
│   │   ├── IChannel.h         # Abstract channel interface
│   │   ├── PrimaryChannel.h   # TCP primary channel
│   │   └── BackupChannel.h    # TCP backup channel
│   │
│   └── src/                    # Implementation Files
│       ├── Message.cpp
│       ├── MessageManager.cpp
│       ├── PersistentQueue.cpp
│       ├── DeliveryEngine.cpp
│       ├── ChannelManager.cpp
│       ├── PrimaryChannel.cpp
│       └── BackupChannel.cpp
│
├── server/                     # Server Application
│   └── server_main.cpp        # TCP server that receives and ACKs messages
│
├── client/                     # Client Demo Application
│   └── client_main.cpp        # Demo showing library usage
│
└── build/                      # Build directory (created by user)
    ├── libReliNetLib.a        # Compiled library (after build)
    ├── ReliNetServer          # Server executable (after build)
    ├── ReliNetClient          # Client executable (after build)
    └── ...                     # CMake and build artifacts
```

## File Descriptions

### Documentation Files

- **README.md** - Overview, features, usage examples, protocol description
- **QUICKSTART.md** - 5-minute integration guide with code examples
- **BUILD.md** - Detailed build instructions for all platforms
- **ARCHITECTURE.md** - System architecture, data flow, design decisions

### Library Headers (lib/include/)

| File | Purpose | Key Classes/Types |
|------|---------|-------------------|
| Message.h | Message representation | `Message`, `Priority`, `MessageStatus` |
| MessageManager.h | Public API for sending | `MessageManager::sendMessage()` |
| PersistentQueue.h | Database storage | `PersistentQueue` with SQLite |
| DeliveryEngine.h | Delivery logic | `DeliveryEngine` with retry/ACK |
| ChannelManager.h | Channel coordination | `ChannelManager` |
| IChannel.h | Channel interface | `IChannel` (abstract) |
| PrimaryChannel.h | Primary TCP channel | `PrimaryChannel` |
| BackupChannel.h | Backup TCP channel | `BackupChannel` |

### Library Implementation (lib/src/)

All `.cpp` files implement the corresponding `.h` headers with business logic.

### Applications

- **server/server_main.cpp** - Standalone server application
  - Listens on two ports (primary: 12345, backup: 12346)
  - Receives messages and sends ACKs
  - Handles duplicate detection
  - Can be used as-is or as a reference

- **client/client_main.cpp** - Demo client application
  - Shows complete library integration
  - Sends test messages with different priorities
  - Demonstrates all features
  - Good starting point for your own client

### Build System

- **CMakeLists.txt** - CMake configuration
  - Builds ReliNetLib as a library
  - Compiles server and client executables
  - Handles Qt dependencies
  - Configures installation

## Component Dependencies

```
ReliNetClient (executable)
    ↓
ReliNetLib (library)
    ↓
Qt6::Core + Qt6::Network + Qt6::Sql

ReliNetServer (executable)
    ↓
Qt6::Core + Qt6::Network
```

## Key Design Decisions

### 1. Library as Separate Target
The core functionality is in `ReliNetLib`, making it easy to integrate into other projects.

### 2. Clean Interface Separation
Public headers in `include/`, implementation in `src/`. Users only need to include headers.

### 3. Platform-Independent
Uses Qt for cross-platform compatibility (Linux, macOS, Windows).

### 4. Single Responsibility
Each class has one clear purpose, making the code maintainable and testable.

### 5. Extensible Design
Abstract interfaces (`IChannel`) allow adding new channel types without modifying core code.

## Usage Patterns

### Pattern 1: Use as Library
```cmake
add_subdirectory(ReliNet)
target_link_libraries(MyApp PRIVATE ReliNetLib)
```

### Pattern 2: Copy Source Files
Copy `lib/include/` and `lib/src/` to your project and add to your build system.

### Pattern 3: Use Executables
Build and run `ReliNetServer` and `ReliNetClient` as standalone applications.

## Database Files

Runtime files created by the applications:

- `relinet_client.db` - Client-side message queue (SQLite)
- `relinet_server.db` - Server-side storage (if implemented)

These are created automatically on first run.

## Lines of Code

Approximate distribution:

| Component | Files | Lines |
|-----------|-------|-------|
| Headers | 8 | ~400 |
| Implementation | 7 | ~900 |
| Server | 1 | ~100 |
| Client | 1 | ~100 |
| CMake | 1 | ~60 |
| Documentation | 4 | ~1400 |
| **Total** | **21** | **~3000** |

## Extension Points

To extend ReliNet:

1. **Add new channel** - Implement `IChannel`, add to `ChannelManager`
2. **Custom storage** - Implement queue interface with different backend
3. **Encryption** - Add encryption layer in channels
4. **Compression** - Compress payload in `Message::serialize()`
5. **Metrics** - Add monitoring signals to `DeliveryEngine`

## Testing Structure (Future)

Recommended test organization:

```
tests/
├── unit/
│   ├── test_message.cpp
│   ├── test_queue.cpp
│   └── test_delivery.cpp
├── integration/
│   ├── test_client_server.cpp
│   └── test_failover.cpp
└── CMakeLists.txt
```

## Build Artifacts

After successful build:

```
build/
├── lib/
│   └── libReliNetLib.a              # Static library
├── ReliNetServer                     # Server executable
├── ReliNetClient                     # Client executable
└── CMakeFiles/                       # Build system files
```

## Integration Examples

See:
- `client/client_main.cpp` - Full client integration
- `QUICKSTART.md` - Code snippets for common tasks
- `README.md` - API usage examples

## Maintenance

Files to update when:

| Change | Files to Update |
|--------|----------------|
| Add new class | CMakeLists.txt, corresponding .h and .cpp |
| Change API | Headers in lib/include/, update QUICKSTART.md |
| New feature | ARCHITECTURE.md, README.md |
| Build requirement | BUILD.md, CMakeLists.txt |

---

**Note**: This is a well-structured, production-ready library design that follows Qt and C++ best practices.
