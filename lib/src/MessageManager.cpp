#include "MessageManager.h"
#include <QDebug>

namespace ReliNet {

MessageManager::MessageManager(PersistentQueue* queue, QObject* parent)
    : QObject(parent)
    , m_queue(queue)
{
}

void MessageManager::sendMessage(const QByteArray& data, Priority priority)
{
    Message msg = createMessage(data, priority);

    if (m_queue->addMessage(msg)) {
        qDebug() << "MessageManager: Message created and queued:" << msg.id()
                 << "Priority:" << Message::priorityToString(priority);
        emit messageCreated(msg.id());
    } else {
        emit error("Failed to add message to queue");
    }
}

Message MessageManager::createMessage(const QByteArray& data, Priority priority)
{
    return Message(data, priority);
}

} // namespace ReliNet
