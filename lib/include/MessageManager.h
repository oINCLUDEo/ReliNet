#ifndef MESSAGEMANAGER_H
#define MESSAGEMANAGER_H

#include "Message.h"
#include "PersistentQueue.h"
#include <QObject>

namespace ReliNet {

class MessageManager : public QObject {
    Q_OBJECT

public:
    explicit MessageManager(PersistentQueue* queue, QObject* parent = nullptr);
    ~MessageManager() override = default;

    void sendMessage(const QByteArray& data, Priority priority = Priority::NORMAL);
    Message createMessage(const QByteArray& data, Priority priority = Priority::NORMAL);

signals:
    void messageCreated(const QString& messageId);
    void error(const QString& errorMessage);

private:
    PersistentQueue* m_queue;
};

} // namespace ReliNet

#endif // MESSAGEMANAGER_H
