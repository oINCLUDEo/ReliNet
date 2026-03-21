#include <gtest/gtest.h>
#include "protocol.hpp"

using namespace ReliNet;

class ProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ProtocolTest, MessageCreation) {
    QByteArray payload("Hello, World!");
    Message msg(12345, Priority::URGENT, ContextTag::MARITIME, payload);
    
    EXPECT_EQ(msg.header.message_id, 12345);
    EXPECT_EQ(msg.header.priority, Priority::URGENT);
    EXPECT_EQ(msg.header.context_tag, ContextTag::MARITIME);
    EXPECT_EQ(msg.payload, payload);
    EXPECT_TRUE(msg.validateChecksum());
}

TEST_F(ProtocolTest, Serialization) {
    QByteArray payload("Test data");
    Message original(54321, Priority::SOS, ContextTag::MILITARY, payload);
    
    auto serialized = ProtocolCodec::serialize(original);
    ASSERT_TRUE(serialized.has_value());
    
    auto deserialized = ProtocolCodec::deserialize(*serialized);
    ASSERT_TRUE(deserialized.has_value());
    
    EXPECT_EQ(deserialized->header.message_id, original.header.message_id);
    EXPECT_EQ(deserialized->header.priority, original.header.priority);
    EXPECT_EQ(deserialized->header.context_tag, original.header.context_tag);
    EXPECT_EQ(deserialized->payload, original.payload);
    EXPECT_TRUE(deserialized->validateChecksum());
}

TEST_F(ProtocolTest, EmergencyPriority) {
    EXPECT_TRUE(ProtocolCodec::isPriorityEmergency(Priority::SOS));
    EXPECT_TRUE(ProtocolCodec::isPriorityEmergency(Priority::DISTRESS));
    EXPECT_FALSE(ProtocolCodec::isPriorityEmergency(Priority::URGENT));
    EXPECT_FALSE(ProtocolCodec::isPriorityEmergency(Priority::ROUTINE));
}

TEST_F(ProtocolTest, InvalidData) {
    QByteArray invalid_data("Not a valid message");
    auto result = ProtocolCodec::deserialize(invalid_data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ProtocolTest, AckMessage) {
    AckMessage ack;
    ack.original_message_id = 98765;
    
    auto serialized = ack.serialize();
    auto deserialized = AckMessage::deserialize(serialized);
    
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->original_message_id, 98765);
}

TEST_F(ProtocolTest, HeartbeatMessage) {
    HeartbeatMessage hb;
    hb.type = HeartbeatMessage::PING;
    hb.timestamp = 1234567890;
    
    auto serialized = hb.serialize();
    auto deserialized = HeartbeatMessage::deserialize(serialized);
    
    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->type, HeartbeatMessage::PING);
    EXPECT_EQ(deserialized->timestamp, 1234567890);
}