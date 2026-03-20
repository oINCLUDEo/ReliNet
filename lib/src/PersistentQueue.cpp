#include "PersistentQueue.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

namespace ReliNet {

PersistentQueue::PersistentQueue(const QString& dbPath, QObject* parent)
    : QObject(parent)
    , m_dbPath(dbPath)
{
}

PersistentQueue::~PersistentQueue()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool PersistentQueue::initialize()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", "relinet_queue");
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        emit error("Failed to open database: " + m_db.lastError().text());
        return false;
    }

    return createTables();
}

bool PersistentQueue::createTables()
{
    QSqlQuery query(m_db);
    QString createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id TEXT PRIMARY KEY,
            payload TEXT NOT NULL,
            priority TEXT NOT NULL,
            status TEXT NOT NULL,
            retry_count INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createTableSQL)) {
        emit error("Failed to create tables: " + query.lastError().text());
        return false;
    }

    return true;
}

bool PersistentQueue::addMessage(const Message& message)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO messages (id, payload, priority, status, retry_count) "
                  "VALUES (:id, :payload, :priority, :status, :retry_count)");

    query.bindValue(":id", message.id());
    query.bindValue(":payload", QString::fromUtf8(message.payload()));
    query.bindValue(":priority", Message::priorityToString(message.priority()));
    query.bindValue(":status", "PENDING");
    query.bindValue(":retry_count", message.retryCount());

    if (!query.exec()) {
        emit error("Failed to add message: " + query.lastError().text());
        return false;
    }

    emit messageAdded(message.id());
    return true;
}

QList<Message> PersistentQueue::getPendingMessages()
{
    QList<Message> messages;
    QSqlQuery query(m_db);

    query.prepare("SELECT * FROM messages WHERE status = 'PENDING' ORDER BY "
                  "CASE priority "
                  "WHEN 'CRITICAL' THEN 0 "
                  "WHEN 'HIGH' THEN 1 "
                  "WHEN 'NORMAL' THEN 2 "
                  "WHEN 'LOW' THEN 3 END, created_at");

    if (!query.exec()) {
        emit error("Failed to get pending messages: " + query.lastError().text());
        return messages;
    }

    while (query.next()) {
        messages.append(messageFromQuery(query));
    }

    return messages;
}

bool PersistentQueue::markAsSent(const QString& messageId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE messages SET status = 'SENT' WHERE id = :id");
    query.bindValue(":id", messageId);

    if (!query.exec()) {
        emit error("Failed to mark message as sent: " + query.lastError().text());
        return false;
    }

    return true;
}

bool PersistentQueue::markAsAcked(const QString& messageId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE messages SET status = 'ACKED' WHERE id = :id");
    query.bindValue(":id", messageId);

    if (!query.exec()) {
        emit error("Failed to mark message as acked: " + query.lastError().text());
        return false;
    }

    return true;
}

bool PersistentQueue::incrementRetry(const QString& messageId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE messages SET retry_count = retry_count + 1, status = 'PENDING' WHERE id = :id");
    query.bindValue(":id", messageId);

    if (!query.exec()) {
        emit error("Failed to increment retry: " + query.lastError().text());
        return false;
    }

    return true;
}

bool PersistentQueue::removeMessage(const QString& messageId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);

    if (!query.exec()) {
        emit error("Failed to remove message: " + query.lastError().text());
        return false;
    }

    return true;
}

Message PersistentQueue::messageFromQuery(QSqlQuery& query)
{
    QString id = query.value("id").toString();
    QByteArray payload = query.value("payload").toString().toUtf8();
    Priority priority = Message::stringToPriority(query.value("priority").toString());
    int retryCount = query.value("retry_count").toInt();

    Message msg(payload, priority);
    // Note: We need to set the id manually since the constructor generates a new one
    QJsonObject json;
    json["id"] = id;
    json["payload"] = QString::fromUtf8(payload);
    json["priority"] = Message::priorityToString(priority);

    Message result(json);
    for (int i = 0; i < retryCount; ++i) {
        result.incrementRetryCount();
    }

    return result;
}

} // namespace ReliNet
