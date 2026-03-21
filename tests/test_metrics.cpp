#include <gtest/gtest.h>
#include "metrics.hpp"

using namespace ReliNet;

class MetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        metrics = new Metrics;
    }
    void TearDown() override {
        delete metrics;
    }
    
    Metrics* metrics;
};

TEST_F(MetricsTest, InitialValues) {
    auto stats = metrics->getStats();
    
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_acked, 0);
    EXPECT_EQ(stats.messages_failed, 0);
    EXPECT_EQ(stats.retries_total, 0);
    EXPECT_EQ(stats.queue_depth, 0);
    EXPECT_EQ(stats.failover_count, 0);
}

TEST_F(MetricsTest, CounterIncrements) {
    metrics->incrementMessagesSent();
    metrics->incrementMessagesAcked();
    metrics->incrementRetries();
    metrics->incrementFailovers();
    
    auto stats = metrics->getStats();
    
    EXPECT_EQ(stats.messages_sent, 1);
    EXPECT_EQ(stats.messages_acked, 1);
    EXPECT_EQ(stats.retries_total, 1);
    EXPECT_EQ(stats.failover_count, 1);
}

TEST_F(MetricsTest, StateUpdates) {
    metrics->setQueueDepth(5);
    metrics->setRtt(250);
    metrics->setLinkState("GOOD");
    metrics->setActiveEndpoint("primary");
    
    auto stats = metrics->getStats();
    
    EXPECT_EQ(stats.queue_depth, 5);
    EXPECT_EQ(stats.rtt_ms, 250);
    EXPECT_EQ(stats.link_state, "GOOD");
    EXPECT_EQ(stats.active_endpoint, "primary");
}

TEST_F(MetricsTest, DeliveryRateCalculation) {
    // Send 10 messages, ack 8
    for (int i = 0; i < 10; ++i) {
        metrics->incrementMessagesSent();
    }
    for (int i = 0; i < 8; ++i) {
        metrics->incrementMessagesAcked();
    }
    metrics->incrementMessagesFailed(); // 1 failed
    
    auto stats = metrics->getStats();
    
    EXPECT_EQ(stats.messages_sent, 10);
    EXPECT_EQ(stats.messages_acked, 8);
    EXPECT_EQ(stats.messages_failed, 1);
    
    // Delivery rate = 8/10 = 80%
    float delivery_rate = static_cast<float>(stats.messages_acked) / stats.messages_sent;
    EXPECT_FLOAT_EQ(delivery_rate, 0.8f);
}