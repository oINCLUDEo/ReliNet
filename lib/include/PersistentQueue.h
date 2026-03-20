#ifndef PERSISTENTQUEUE_H
#define PERSISTENTQUEUE_H

#include "Message.h"
#include <QObject>
#include <QSqlDatabase>
#include <QList>

namespace ReliNet {

class PersistentQueue : public QObject {
    Q_OBJECT

public:
    explicit PersistentQueue(const QString& dbPath, QObject* parent = nullptr);
    ~PersistentQueue();

    bool initialize();
    bool addMessage(const Message& message);
    QList<Message> getPendingMessages();
    bool markAsSent(const QString& messageId);
    bool markAsAcked(const QString& messageId);
    bool incrementRetry(const QString& messageId);
    bool removeMessage(const QString& messageId);

signals:
    void messageAdded(const QString& messageId);
    void error(const QString& errorMessage);

private:
    bool createTables();
    Message messageFromQuery(class QSqlQuery& query);

    QString m_dbPath;
    QSqlDatabase m_db;
};

} // namespace ReliNet

#endif // PERSISTENTQUEUE_H
