cmake_minimum_required(VERSION 3.16)

project(ReliNet VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try Qt6 first, fallback to Qt5
find_package(Qt6 QUIET COMPONENTS Core Network Widgets SerialPort)
if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Core Network Widgets SerialPort)
    set(QT_VERSION 5)
    message(STATUS "Using Qt5")
else()
    set(QT_VERSION 6)
    message(STATUS "Using Qt6")
endif()

# Find dependencies (make optional for compilation test)
find_package(PkgConfig)

# Find GoogleTest (optional for this test)
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GoogleTest not found - tests will not be built")
endif()

if(QT_VERSION EQUAL 6)
    qt_standard_project_setup(REQUIRES 6.5)
elseif(QT_VERSION EQUAL 5)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

# Compiler flags
add_compile_options(-Wall -Wextra)

# ReliNet Library - simplified for compilation test
set(RELINET_SOURCES
    src/protocol.cpp
    src/retry_engine.cpp
    src/outbox_queue.cpp
    src/tcp_transport.cpp
    src/udp_transport.cpp
    src/serial_transport.cpp
    src/satellite_transport.cpp
    src/bluetooth_transport_stub.cpp
    src/endpoint_manager.cpp
    src/protocol_fallback.cpp
    src/heartbeat.cpp
    src/link_monitor.cpp
    src/adaptive_sender.cpp
    src/mainwindow.cpp
    src/theme_manager.cpp
    src/metrics.cpp
    # Headers with Q_OBJECT must be listed so AUTOMOC processes them
    include/protocol.hpp
    include/retry_engine.hpp
    include/outbox_queue.hpp
    include/itransport.hpp
    include/tcp_transport.hpp
    include/serial_transport.hpp
    include/bluetooth_transport.hpp
    include/endpoint_manager.hpp
    include/protocol_fallback.hpp
    include/heartbeat.hpp
    include/link_monitor.hpp
    include/mainwindow.hpp
    include/theme_manager.hpp
    include/metrics.hpp
)

add_library(relinet ${RELINET_SOURCES})

target_include_directories(relinet PUBLIC include)

if(QT_VERSION EQUAL 6)
    target_link_libraries(relinet PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Widgets
        Qt6::SerialPort
    )
else()
    target_link_libraries(relinet PUBLIC
        Qt5::Core
        Qt5::Network
        Qt5::Widgets
        Qt5::SerialPort
    )
endif()

# GUI Application  
add_executable(appReliNet main.cpp)
target_link_libraries(appReliNet PRIVATE relinet)

# Demo Application
add_executable(reliNet_demo demo/main_demo.cpp)
target_link_libraries(reliNet_demo PRIVATE relinet)

# Tests (only if GoogleTest found)
if(GTest_FOUND)
    enable_testing()
    
    set(TEST_SOURCES
        tests/test_protocol.cpp
        tests/test_retry_engine.cpp
        tests/test_outbox_queue.cpp
        tests/test_endpoint_manager.cpp
        tests/test_metrics.cpp
    )
    
    add_executable(relinet_tests ${TEST_SOURCES})
    target_link_libraries(relinet_tests PRIVATE relinet gtest_main)
    
    include(GoogleTest)
    gtest_discover_tests(relinet_tests)
endif()

# Install
include(GNUInstallDirs)
install(TARGETS appReliNet relinet reliNet_demo
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
