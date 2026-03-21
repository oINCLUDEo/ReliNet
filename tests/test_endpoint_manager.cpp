#include <gtest/gtest.h>
#include <QSignalSpy>
#include "endpoint_manager.hpp"

using namespace ReliNet;

class EndpointManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = new EndpointManager;
    }
    void TearDown() override {
        delete manager;
    }
    
    EndpointManager* manager;
};

TEST_F(EndpointManagerTest, AddEndpoint) {
    QSignalSpy spy(manager, &EndpointManager::endpointAdded);
    
    Endpoint ep("test1", "Test Endpoint", QStringList{"192.168.1.1"}, 
                8080, TransportType::TCP, 100, DeploymentContext::GENERAL);
    manager->addEndpoint(ep);
    
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(manager->getActiveEndpointId(), "test1");
}

TEST_F(EndpointManagerTest, Failover) {
    // Add primary and backup endpoints
    Endpoint primary("primary", "Primary", QStringList{"192.168.1.1"}, 
                    8080, TransportType::TCP, 100, DeploymentContext::GENERAL);
    Endpoint backup("backup", "Backup", QStringList{"192.168.1.2"}, 
                   8080, TransportType::TCP, 80, DeploymentContext::GENERAL);
    
    manager->addEndpoint(primary);
    manager->addEndpoint(backup);
    
    QSignalSpy failoverSpy(manager, &EndpointManager::endpointSwitched);
    
    // Trigger failover from primary
    manager->handleHeartbeatFailure("primary");
    
    EXPECT_EQ(failoverSpy.count(), 1);
    EXPECT_EQ(manager->getActiveEndpointId(), "backup");
}

TEST_F(EndpointManagerTest, EmergencyBroadcast) {
    manager->setEmergencyBroadcastMode(true);
    EXPECT_TRUE(manager->isEmergencyBroadcastActive());
    
    manager->setEmergencyBroadcastMode(false);
    EXPECT_FALSE(manager->isEmergencyBroadcastActive());
}

TEST_F(EndpointManagerTest, ReachabilityTracking) {
    Endpoint ep("test", "Test", QStringList{"192.168.1.1"}, 
                8080, TransportType::TCP, 100, DeploymentContext::GENERAL);
    manager->addEndpoint(ep);
    
    QSignalSpy spy(manager, &EndpointManager::endpointReachabilityChanged);
    
    manager->updateEndpointReachability("test", false);
    EXPECT_EQ(spy.count(), 1);
    
    auto args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), "test");
    EXPECT_FALSE(args.at(1).toBool());
}