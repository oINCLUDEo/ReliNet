#include <gtest/gtest.h>
#include <QTemporaryFile>
#include "outbox_queue.hpp"

using namespace ReliNet;

class OutboxQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_file = new QTemporaryFile;
        temp_file->open();
        queue = new OutboxQueue(temp_file->fileName());
    }
    
    void TearDown() override {
        delete queue;
        delete temp_file;
    }
    
    QTemporaryFile* temp_file;
    OutboxQueue* queue;
};

TEST_F(OutboxQueueTest, EnqueueMessage) {
    Message msg(111, Priority::ROUTINE, ContextTag::GENERAL, QByteArray("test"));
    EXPECT_TRUE(queue->enqueue(msg));
    EXPECT_EQ(queue->getPendingCount(), 1);
}

TEST_F(OutboxQueueTest, MarkAcked) {
    Message msg(222, Priority::URGENT, ContextTag::MARITIME, QByteArray("test"));
    queue->enqueue(msg);
    
    EXPECT_TRUE(queue->markAcked(222));
    EXPECT_EQ(queue->getPendingCount(), 0);
}

TEST_F(OutboxQueueTest, RecoveryPriority) {
    // Add messages with different priorities
    Message routine(1, Priority::ROUTINE, ContextTag::GENERAL, QByteArray("routine"));
    Message sos(2, Priority::SOS, ContextTag::MARITIME, QByteArray("sos"));
    Message urgent(3, Priority::URGENT, ContextTag::MILITARY, QByteArray("urgent"));
    
    queue->enqueue(routine);
    queue->enqueue(sos);
    queue->enqueue(urgent);
    
    auto pending = queue->recoverPendingMessages();
    EXPECT_EQ(pending.size(), 3);
    
    // SOS should be first
    EXPECT_EQ(pending[0].message_id, 2);
    EXPECT_EQ(pending[0].priority, Priority::SOS);
}

TEST_F(OutboxQueueTest, Compaction) {
    Message msg1(101, Priority::ROUTINE, ContextTag::GENERAL, QByteArray("msg1"));
    Message msg2(102, Priority::ROUTINE, ContextTag::GENERAL, QByteArray("msg2"));
    
    queue->enqueue(msg1);
    queue->enqueue(msg2);
    queue->markAcked(101);
    
    EXPECT_TRUE(queue->compactFile());
    EXPECT_EQ(queue->getPendingCount(), 1);
}