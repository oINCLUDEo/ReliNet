#!/bin/bash
# Simple build script for ReliNet

set -e  # Exit on error

echo "=== Building ReliNet ==="

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
    echo "Created build directory"
fi

cd build

# Configure with CMake
echo ""
echo "Configuring with CMake..."
cmake ..

# Build
echo ""
echo "Building..."
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== Build completed successfully! ==="
echo ""
echo "Executables:"
echo "  - Server: ./build/ReliNetServer"
echo "  - Client: ./build/ReliNetClient"
echo ""
echo "To run:"
echo "  Terminal 1: ./build/ReliNetServer"
echo "  Terminal 2: ./build/ReliNetClient"
