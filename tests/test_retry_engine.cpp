#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QEventLoop>
#include "retry_engine.hpp"

using namespace ReliNet;

class RetryEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = new RetryEngine;
    }
    void TearDown() override {
        delete engine;
    }
    
    RetryEngine* engine;
};

TEST_F(RetryEngineTest, SendMessage) {
    QSignalSpy spy(engine, &RetryEngine::sendRequest);
    
    Message msg(123, Priority::ROUTINE, ContextTag::GENERAL, QByteArray("test"));
    engine->sendMessage(msg);
    
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(engine->getInFlightCount(), 1);
}

TEST_F(RetryEngineTest, HandleAck) {
    QSignalSpy deliveredSpy(engine, &RetryEngine::deliveryConfirmed);
    
    Message msg(456, Priority::URGENT, ContextTag::MARITIME, QByteArray("test"));
    engine->sendMessage(msg);
    engine->handleAck(456);
    
    EXPECT_EQ(deliveredSpy.count(), 1);
    EXPECT_EQ(engine->getInFlightCount(), 0);
}

TEST_F(RetryEngineTest, Deduplication) {
    EXPECT_TRUE(engine->shouldProcessMessage(789));
    EXPECT_FALSE(engine->shouldProcessMessage(789)); // Duplicate
}

TEST_F(RetryEngineTest, EmergencyPriorityConfig) {
    RetryConfig config;
    config.sos_max_retries = -1; // Unlimited
    engine->setConfig(config);
    
    // SOS messages should have unlimited retries
    EXPECT_EQ(engine->getConfig().sos_max_retries, -1);
}