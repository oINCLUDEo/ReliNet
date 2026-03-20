#include "DeliveryEngine.h"
#include <QDebug>
#include <algorithm>

namespace ReliNet {

DeliveryEngine::DeliveryEngine(PersistentQueue* queue, ChannelManager* channelManager, QObject* parent)
    : QObject(parent)
    , m_queue(queue)
    , m_channelManager(channelManager)
    , m_processTimer(new QTimer(this))
    , m_ackTimeout(5000)     // 5 seconds default
    , m_maxRetries(5)
    , m_processInterval(1000) // 1 second default
{
    connect(m_processTimer, &QTimer::timeout, this, &DeliveryEngine::processQueue);

    // Connect to all channels for ACK reception
    for (IChannel* channel : m_channelManager->getAllChannels()) {
        connect(channel, &IChannel::ackReceived, this, &DeliveryEngine::onAckReceived);
        connect(channel, &IChannel::messageSent, this, &DeliveryEngine::onMessageSent);
        connect(channel, &IChannel::sendFailed, this, &DeliveryEngine::onSendFailed);
    }
}

DeliveryEngine::~DeliveryEngine()
{
    stop();
    // Clean up pending timers
    for (auto& pending : m_pendingMessages) {
        if (pending.ackTimer) {
            pending.ackTimer->stop();
            delete pending.ackTimer;
        }
    }
}

void DeliveryEngine::start()
{
    qDebug() << "DeliveryEngine: Starting";
    m_processTimer->start(m_processInterval);
    processQueue(); // Process immediately
}

void DeliveryEngine::stop()
{
    qDebug() << "DeliveryEngine: Stopping";
    m_processTimer->stop();
}

void DeliveryEngine::setProcessInterval(int milliseconds)
{
    m_processInterval = milliseconds;
    if (m_processTimer->isActive()) {
        m_processTimer->setInterval(milliseconds);
    }
}

void DeliveryEngine::processQueue()
{
    QList<Message> pendingMessages = m_queue->getPendingMessages();

    for (const Message& msg : pendingMessages) {
        // Skip if already being processed
        if (m_pendingMessages.contains(msg.id())) {
            continue;
        }

        // Check retry limit
        if (msg.retryCount() >= m_maxRetries) {
            qDebug() << "DeliveryEngine: Message" << msg.id() << "exceeded max retries";
            emit messageFailed(msg.id(), "Max retries exceeded");
            m_queue->removeMessage(msg.id());
            continue;
        }

        sendMessage(msg);
    }
}

void DeliveryEngine::sendMessage(const Message& msg)
{
    IChannel* channel = selectChannel(msg, msg.retryCount());

    if (!channel) {
        qDebug() << "DeliveryEngine: No available channel for message" << msg.id();
        return;
    }

    qDebug() << "DeliveryEngine: Sending message" << msg.id()
             << "via" << channel->name()
             << "(retry" << msg.retryCount() << ")";

    // For CRITICAL priority, send via multiple channels
    if (msg.priority() == Priority::CRITICAL) {
        QList<IChannel*> availableChannels = m_channelManager->getAvailableChannels();
        for (IChannel* ch : availableChannels) {
            ch->send(msg);
        }
    } else {
        channel->send(msg);
    }

    // Track pending message
    PendingMessage pending;
    pending.message = msg;
    pending.currentRetry = msg.retryCount();
    pending.lastChannel = channel;
    pending.ackTimer = nullptr;

    m_pendingMessages[msg.id()] = pending;
    m_queue->markAsSent(msg.id());
}

IChannel* DeliveryEngine::selectChannel(const Message& msg, int retryCount)
{
    IChannel* primary = m_channelManager->getPrimaryChannel();
    IChannel* backup = m_channelManager->getBackupChannel();

    // Strategy based on priority and retry count
    switch (msg.priority()) {
        case Priority::LOW:
        case Priority::NORMAL:
            // Always use primary if available
            if (primary && primary->isAvailable()) {
                return primary;
            }
            // Fallback to backup
            if (backup && backup->isAvailable()) {
                return backup;
            }
            break;

        case Priority::HIGH:
            // Try primary first, switch to backup on retry
            if (retryCount == 0) {
                if (primary && primary->isAvailable()) {
                    return primary;
                }
            }
            // Use backup after first failure
            if (backup && backup->isAvailable()) {
                return backup;
            }
            if (primary && primary->isAvailable()) {
                return primary;
            }
            break;

        case Priority::CRITICAL:
            // Return any available channel (will send via all in sendMessage)
            if (primary && primary->isAvailable()) {
                return primary;
            }
            if (backup && backup->isAvailable()) {
                return backup;
            }
            break;
    }

    return nullptr;
}

void DeliveryEngine::setupAckTimer(const QString& messageId)
{
    if (!m_pendingMessages.contains(messageId)) {
        return;
    }

    PendingMessage& pending = m_pendingMessages[messageId];

    // Calculate backoff
    int timeout = m_ackTimeout + calculateBackoff(pending.currentRetry);

    pending.ackTimer = new QTimer(this);
    pending.ackTimer->setSingleShot(true);
    pending.ackTimer->setInterval(timeout);

    connect(pending.ackTimer, &QTimer::timeout, this, [this, messageId]() {
        handleAckTimeout(messageId);
    });

    pending.ackTimer->start();
}

void DeliveryEngine::handleAckTimeout(const QString& messageId)
{
    if (!m_pendingMessages.contains(messageId)) {
        return;
    }

    qDebug() << "DeliveryEngine: ACK timeout for message" << messageId;

    PendingMessage pending = m_pendingMessages[messageId];
    m_pendingMessages.remove(messageId);

    if (pending.ackTimer) {
        delete pending.ackTimer;
    }

    // Increment retry count in database
    m_queue->incrementRetry(messageId);

    emit retryingMessage(messageId, pending.currentRetry + 1);
}

int DeliveryEngine::calculateBackoff(int retryCount)
{
    // Exponential backoff: 0, 1000, 2000, 4000, 8000, ...
    return std::min(1000 * (1 << retryCount), 30000); // Max 30 seconds
}

void DeliveryEngine::onAckReceived(const QString& messageId)
{
    if (!m_pendingMessages.contains(messageId)) {
        return;
    }

    qDebug() << "DeliveryEngine: ACK received for message" << messageId;

    PendingMessage pending = m_pendingMessages[messageId];
    m_pendingMessages.remove(messageId);

    if (pending.ackTimer) {
        pending.ackTimer->stop();
        delete pending.ackTimer;
    }

    m_queue->markAsAcked(messageId);
    emit messageDelivered(messageId);
}

void DeliveryEngine::onMessageSent(const QString& messageId)
{
    // Start ACK timer when message is sent
    setupAckTimer(messageId);
}

void DeliveryEngine::onSendFailed(const QString& messageId, const QString& error)
{
    qDebug() << "DeliveryEngine: Send failed for message" << messageId << ":" << error;

    if (m_pendingMessages.contains(messageId)) {
        PendingMessage pending = m_pendingMessages[messageId];
        m_pendingMessages.remove(messageId);

        if (pending.ackTimer) {
            delete pending.ackTimer;
        }

        // Will be retried on next process cycle
        m_queue->incrementRetry(messageId);
    }
}

void DeliveryEngine::onAckTimeout()
{
    // This is handled by lambda in setupAckTimer
}

} // namespace ReliNet
