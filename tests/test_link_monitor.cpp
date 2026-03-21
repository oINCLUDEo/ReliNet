#include <gtest/gtest.h>
#include <QSignalSpy>
#include "link_monitor.hpp"

using namespace ReliNet;

class LinkMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = new LinkMonitor;
    }
    void TearDown() override {
        delete monitor;
    }
    
    LinkMonitor* monitor;
};

TEST_F(LinkMonitorTest, InitialState) {
    EXPECT_EQ(monitor->getLinkState(), LinkState::GOOD);
    EXPECT_EQ(monitor->getAverageRtt(), 0);
    EXPECT_FLOAT_EQ(monitor->getLossRate(), 0.0f);
}

TEST_F(LinkMonitorTest, RTTTracking) {
    monitor->recordRtt(100);
    monitor->recordRtt(200);
    monitor->recordRtt(150);
    
    EXPECT_EQ(monitor->getAverageRtt(), 150);
    EXPECT_EQ(monitor->getLinkState(), LinkState::GOOD);
}

TEST_F(LinkMonitorTest, LinkStateTransitions) {
    QSignalSpy spy(monitor, &LinkMonitor::linkStateChanged);
    
    // Force POOR state with high RTT
    for (int i = 0; i < 10; ++i) {
        monitor->recordRtt(5000); // Very high RTT
    }
    
    EXPECT_EQ(monitor->getLinkState(), LinkState::POOR);
    EXPECT_GT(spy.count(), 0);
}

TEST_F(LinkMonitorTest, LossRateCalculation) {
    // Send some messages
    for (int i = 0; i < 10; ++i) {
        monitor->recordMessageSent();
    }
    
    // Lose 2 messages
    monitor->recordMessageLost();
    monitor->recordMessageLost();
    
    EXPECT_FLOAT_EQ(monitor->getLossRate(), 20.0f); // 2/10 = 20%
}

class AdaptiveSenderTest : public ::testing::Test {
protected:
    void SetUp() override {
        sender = new AdaptiveSender;
    }
    void TearDown() override {
        delete sender;
    }
    
    AdaptiveSender* sender;
};

TEST_F(AdaptiveSenderTest, EmergencyBypassesBatching) {
    Message sos_msg(123, Priority::SOS, ContextTag::MARITIME, QByteArray("SOS"));
    Message distress_msg(124, Priority::DISTRESS, ContextTag::MARITIME, QByteArray("DISTRESS"));
    
    // Emergency messages should never be batched, regardless of link state
    EXPECT_FALSE(sender->shouldBatch(sos_msg, LinkState::POOR));
    EXPECT_FALSE(sender->shouldBatch(distress_msg, LinkState::POOR));
    EXPECT_FALSE(sender->shouldBatch(sos_msg, LinkState::GOOD));
    EXPECT_FALSE(sender->shouldBatch(distress_msg, LinkState::GOOD));
}

TEST_F(AdaptiveSenderTest, UrgentBatchingRules) {
    Message urgent_msg(125, Priority::URGENT, ContextTag::MILITARY, QByteArray("URGENT"));
    
    // URGENT bypasses batching except in POOR state
    EXPECT_FALSE(sender->shouldBatch(urgent_msg, LinkState::GOOD));
    EXPECT_FALSE(sender->shouldBatch(urgent_msg, LinkState::DEGRADED));
    EXPECT_TRUE(sender->shouldBatch(urgent_msg, LinkState::POOR));
}

TEST_F(AdaptiveSenderTest, RoutineBatchingRules) {
    Message routine_msg(126, Priority::ROUTINE, ContextTag::GENERAL, QByteArray("ROUTINE"));
    
    // ROUTINE batches in DEGRADED and POOR states
    EXPECT_FALSE(sender->shouldBatch(routine_msg, LinkState::GOOD));
    EXPECT_TRUE(sender->shouldBatch(routine_msg, LinkState::DEGRADED));
    EXPECT_TRUE(sender->shouldBatch(routine_msg, LinkState::POOR));
}