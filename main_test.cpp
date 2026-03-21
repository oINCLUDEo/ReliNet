#include <iostream>
#include "protocol.hpp"
#include "metrics.hpp"

using namespace ReliNet;

int main() {
    std::cout << "=== ReliNet Basic Compilation Test ===" << std::endl;
    
    // Test protocol
    QByteArray test_payload("Hello World");
    Message msg(123, Priority::ROUTINE, ContextTag::GENERAL, test_payload);
    
    std::cout << "Message ID: " << msg.header.message_id << std::endl;
    std::cout << "Priority: " << ProtocolCodec::priorityToString(msg.header.priority).toStdString() << std::endl;
    std::cout << "Context: " << ProtocolCodec::contextTagToString(msg.header.context_tag).toStdString() << std::endl;
    
    // Test serialization
    auto serialized = ProtocolCodec::serialize(msg);
    if (serialized.has_value()) {
        std::cout << "Serialization successful: " << serialized->size() << " bytes" << std::endl;
        
        auto deserialized = ProtocolCodec::deserialize(*serialized);
        if (deserialized.has_value()) {
            std::cout << "Deserialization successful" << std::endl;
            std::cout << "Checksum valid: " << (deserialized->validateChecksum() ? "YES" : "NO") << std::endl;
        } else {
            std::cout << "Deserialization failed" << std::endl;
        }
    } else {
        std::cout << "Serialization failed" << std::endl;
    }
    
    // Test metrics
    Metrics metrics;
    metrics.incrementMessagesSent();
    metrics.incrementMessagesAcked();
    metrics.setRtt(150);
    
    auto stats = metrics.getStats();
    std::cout << "Messages sent: " << stats.messages_sent << std::endl;
    std::cout << "Messages acked: " << stats.messages_acked << std::endl;
    std::cout << "RTT: " << stats.rtt_ms << "ms" << std::endl;
    
    std::cout << "\n=== Basic compilation test completed successfully ===" << std::endl;
    return 0;
}