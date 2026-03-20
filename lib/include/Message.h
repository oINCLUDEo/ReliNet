#ifndef MESSAGE_H
#define MESSAGE_H

#include <QByteArray>
#include <QString>
#include <QJsonObject>
#include <QUuid>

namespace ReliNet {

enum class Priority {
    LOW,
    NORMAL,
    HIGH,
    CRITICAL
};

enum class MessageStatus {
    PENDING,
    SENT,
    ACKED
};

class Message {
public:
    Message();
    Message(const QByteArray& payload, Priority priority);
    Message(const QJsonObject& json);

    QString id() const { return m_id; }
    QByteArray payload() const { return m_payload; }
    Priority priority() const { return m_priority; }
    MessageStatus status() const { return m_status; }
    int retryCount() const { return m_retryCount; }

    void setStatus(MessageStatus status) { m_status = status; }
    void incrementRetryCount() { ++m_retryCount; }

    QJsonObject toJson() const;
    QByteArray serialize() const;

    static Message fromJson(const QJsonObject& json);
    static Priority stringToPriority(const QString& str);
    static QString priorityToString(Priority priority);

private:
    QString m_id;
    QByteArray m_payload;
    Priority m_priority;
    MessageStatus m_status;
    int m_retryCount;
};

} // namespace ReliNet

#endif // MESSAGE_H
