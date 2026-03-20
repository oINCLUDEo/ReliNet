# Build Instructions for ReliNet

## Quick Start (Recommended)

The easiest way to build ReliNet is using the provided build scripts:

### Linux/macOS
```bash
./build.sh
```

### Windows
```cmd
build.bat
```

These scripts will automatically:
- Create the build directory
- Configure CMake
- Build all targets (library, server, client)

## Prerequisites

### Required Software

1. **CMake** (version 3.16 or later)
   - Download from: https://cmake.org/download/

2. **Qt 6** (version 6.2 or later)
   - Download from: https://www.qt.io/download
   - Required modules: Core, Network, Sql
   - You can install Qt via:
     - Qt Online Installer (recommended)
     - Package manager (e.g., `apt install qt6-base-dev` on Ubuntu)
     - vcpkg: `vcpkg install qt6-base qt6-5compat`

3. **C++17 Compliant Compiler**
   - GCC 7+ / Clang 5+ / MSVC 2017+

## Building on Linux/macOS

### Step 1: Install Qt 6

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install qt6-base-dev qt6-base-dev-tools libqt6sql6-sqlite
```

**Fedora:**
```bash
sudo dnf install qt6-qtbase-devel
```

**macOS (with Homebrew):**
```bash
brew install qt@6
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"
```

### Step 2: Build the Project

```bash
cd ReliNet
mkdir build
cd build
cmake ..
cmake --build .
```

### Step 3: Run the Applications

**Terminal 1 - Start the Server:**
```bash
./ReliNetServer
```

**Terminal 2 - Start the Client:**
```bash
./ReliNetClient
```

## Building on Windows

### Step 1: Install Qt 6

1. Download Qt Online Installer from https://www.qt.io/download
2. Install Qt 6 with the following components:
   - Qt 6.x for Desktop (MSVC or MinGW)
   - CMake
   - Ninja (optional)

### Step 2: Configure Environment

Open "Qt 6.x (MSVC/MinGW)" command prompt or add Qt to PATH:
```cmd
set PATH=C:\Qt\6.x\msvc2019_64\bin;%PATH%
set CMAKE_PREFIX_PATH=C:\Qt\6.x\msvc2019_64
```

### Step 3: Build with CMake

```cmd
cd ReliNet
mkdir build
cd build
cmake .. -G "Ninja"
cmake --build .
```

Or use Visual Studio:
```cmd
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### Step 4: Run the Applications

**Command Prompt 1:**
```cmd
ReliNetServer.exe
```

**Command Prompt 2:**
```cmd
ReliNetClient.exe
```

## Using Qt Creator (All Platforms)

1. Open Qt Creator
2. File → Open File or Project
3. Select `CMakeLists.txt` in the ReliNet directory
4. Configure the project with your Qt 6 kit
5. Build (Ctrl+B / Cmd+B)
6. Run ReliNetServer (Ctrl+R / Cmd+R)
7. Run ReliNetClient from a second instance or terminal

## Testing the System

Once both server and client are running, you should see:

**Server Output:**
```
Server listening on port 12345
Server listening on port 12346
ReliNet Server started
Primary port: 12345
Backup port: 12346
New client connected: 127.0.0.1
Message received: <uuid> Priority: HIGH Payload: High priority message
ACK sent for message: <uuid>
```

**Client Output:**
```
=== ReliNet Client Demo ===
Client is running. Press Ctrl+C to exit.
Connecting to servers...
PrimaryChannel: Connected to server
BackupChannel: Connected to server
--- Sending test messages ---
MessageManager: Message created and queued: <uuid> Priority: LOW
DeliveryEngine: Sending message <uuid> via Primary (TCP) (retry 0)
✓ Message delivered: <uuid>
```

## Troubleshooting

### Qt not found
```
CMake Error: Could not find Qt6
```

**Solution:** Set CMAKE_PREFIX_PATH to your Qt installation:
```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64 ..
```

### SQLite module not found
```
Could not find Qt6Sql
```

**Solution:** Install Qt SQL module:
```bash
sudo apt install libqt6sql6-sqlite  # Ubuntu/Debian
```

### Build fails with C++ standard errors
```
error: 'filesystem' is not a member of 'std'
```

**Solution:** Ensure your compiler supports C++17:
```bash
cmake -DCMAKE_CXX_STANDARD=17 ..
```

### Server port already in use
```
QTcpServer: listen: Address already in use
```

**Solution:** Change ports in server_main.cpp or kill existing process:
```bash
lsof -ti:12345 | xargs kill -9  # Linux/macOS
netstat -ano | findstr :12345   # Windows (then use taskkill)
```

## Development Tips

### Clean Build
```bash
rm -rf build
mkdir build
cd build
cmake ..
cmake --build .
```

### Debug Build
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

### Release Build
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Verbose Build
```bash
cmake --build . --verbose
```

## Integration into Your Project

### As a Library

Add to your CMakeLists.txt:
```cmake
add_subdirectory(path/to/ReliNet)
target_link_libraries(YourApp PRIVATE ReliNetLib)
```

### As Source Files

Copy the `lib/` directory to your project and add the files to your build system.

## Next Steps

After successful build:
1. Review the README.md for API usage
2. Check client/client_main.cpp for integration examples
3. Modify server ports if needed
4. Test network resilience by disconnecting/reconnecting
5. Extend with custom channel implementations

## Support

For issues with:
- **Qt Installation:** https://doc.qt.io/qt-6/get-and-install-qt.html
- **CMake:** https://cmake.org/documentation/
- **This Project:** Check the README.md or source code comments
