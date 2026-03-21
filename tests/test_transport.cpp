#include <gtest/gtest.h>
#include "tcp_transport.cpp" // Include implementation for testing
#include <QSignalSpy>

using namespace ReliNet;

class TransportTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TransportTest, TcpCapabilities) {
    TcpTransport transport;
    auto caps = transport.capabilities();
    
    EXPECT_FALSE(caps.multi_homing);
    EXPECT_FALSE(caps.message_boundaries);
    EXPECT_TRUE(caps.reliable);
    EXPECT_TRUE(caps.ordered);
    EXPECT_GT(caps.mtu, 1000);
}

TEST_F(TransportTest, TransportTypeIdentification) {
    TcpTransport tcp_transport;
    EXPECT_EQ(tcp_transport.transportType(), TransportType::TCP);
    EXPECT_EQ(tcp_transport.transportName(), "TCP Socket");
}

TEST_F(TransportTest, InitialState) {
    TcpTransport transport;
    EXPECT_FALSE(transport.isConnected());
    EXPECT_EQ(transport.getConnectionInfo(), "TCP Disconnected");
}

TEST_F(TransportTest, BluetoothStub) {
    BluetoothTransport bluetooth;
    QSignalSpy errorSpy(&bluetooth, &ITransport::errorOccurred);
    
    bluetooth.connectToHost("dummy", 1234);
    
    EXPECT_EQ(errorSpy.count(), 1);
    auto args = errorSpy.takeFirst();
    EXPECT_TRUE(args.at(0).toString().contains("not implemented"));
}