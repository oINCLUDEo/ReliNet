#ifndef DELIVERYENGINE_H
#define DELIVERYENGINE_H

#include "Message.h"
#include "PersistentQueue.h"
#include "ChannelManager.h"
#include <QObject>
#include <QTimer>
#include <QMap>

namespace ReliNet {

struct PendingMessage {
    Message message;
    QTimer* ackTimer;
    int currentRetry;
    IChannel* lastChannel;
};

class DeliveryEngine : public QObject {
    Q_OBJECT

public:
    explicit DeliveryEngine(PersistentQueue* queue, ChannelManager* channelManager, QObject* parent = nullptr);
    ~DeliveryEngine() override;

    void start();
    void stop();
    void processQueue();

    // Configuration
    void setAckTimeout(int milliseconds) { m_ackTimeout = milliseconds; }
    void setMaxRetries(int retries) { m_maxRetries = retries; }
    void setProcessInterval(int milliseconds);

signals:
    void messageDelivered(const QString& messageId);
    void messageFailed(const QString& messageId, const QString& reason);
    void retryingMessage(const QString& messageId, int retryCount);

private slots:
    void onAckReceived(const QString& messageId);
    void onMessageSent(const QString& messageId);
    void onSendFailed(const QString& messageId, const QString& error);
    void onAckTimeout();

private:
    void sendMessage(const Message& msg);
    IChannel* selectChannel(const Message& msg, int retryCount);
    void setupAckTimer(const QString& messageId);
    void handleAckTimeout(const QString& messageId);
    int calculateBackoff(int retryCount);

    PersistentQueue* m_queue;
    ChannelManager* m_channelManager;
    QTimer* m_processTimer;

    QMap<QString, PendingMessage> m_pendingMessages;

    int m_ackTimeout;      // milliseconds
    int m_maxRetries;
    int m_processInterval; // milliseconds
};

} // namespace ReliNet

#endif // DELIVERYENGINE_H
