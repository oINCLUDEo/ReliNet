cmake_minimum_required(VERSION 3.16)

project(ReliNet VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

# ReliNet Library
add_library(ReliNetLib
    # Source files
    lib/src/Message.cpp
    lib/src/MessageManager.cpp
    lib/src/PersistentQueue.cpp
    lib/src/ChannelManager.cpp
    lib/src/PrimaryChannel.cpp
    lib/src/BackupChannel.cpp
    lib/src/DeliveryEngine.cpp
    # Header files (needed for MOC)
    lib/include/Message.h
    lib/include/MessageManager.h
    lib/include/PersistentQueue.h
    lib/include/ChannelManager.h
    lib/include/IChannel.h
    lib/include/PrimaryChannel.h
    lib/include/BackupChannel.h
    lib/include/DeliveryEngine.h
)

target_include_directories(ReliNetLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
)

target_link_libraries(ReliNetLib
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Sql
)

# Server Application
add_executable(ReliNetServer
    server/server_main.cpp
)

target_link_libraries(ReliNetServer
    PRIVATE
        Qt6::Core
        Qt6::Network
)

# Client Application
add_executable(ReliNetClient
    client/client_main.cpp
)

target_link_libraries(ReliNetClient
    PRIVATE
        ReliNetLib
        Qt6::Core
        Qt6::Network
        Qt6::Sql
)

# Installation
include(GNUInstallDirs)
install(TARGETS ReliNetLib ReliNetServer ReliNetClient
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
