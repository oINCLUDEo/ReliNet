#include "Message.h"
#include <QJsonDocument>

namespace ReliNet {

Message::Message()
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_priority(Priority::NORMAL)
    , m_status(MessageStatus::PENDING)
    , m_retryCount(0)
{
}

Message::Message(const QByteArray& payload, Priority priority)
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_payload(payload)
    , m_priority(priority)
    , m_status(MessageStatus::PENDING)
    , m_retryCount(0)
{
}

Message::Message(const QJsonObject& json)
    : m_id(json["id"].toString())
    , m_payload(json["payload"].toString().toUtf8())
    , m_priority(stringToPriority(json["priority"].toString()))
    , m_status(MessageStatus::PENDING)
    , m_retryCount(0)
{
}

QJsonObject Message::toJson() const
{
    QJsonObject obj;
    obj["id"] = m_id;
    obj["payload"] = QString::fromUtf8(m_payload);
    obj["priority"] = priorityToString(m_priority);
    return obj;
}

QByteArray Message::serialize() const
{
    QJsonDocument doc(toJson());
    return doc.toJson(QJsonDocument::Compact);
}

Message Message::fromJson(const QJsonObject& json)
{
    return Message(json);
}

Priority Message::stringToPriority(const QString& str)
{
    if (str == "LOW") return Priority::LOW;
    if (str == "HIGH") return Priority::HIGH;
    if (str == "CRITICAL") return Priority::CRITICAL;
    return Priority::NORMAL;
}

QString Message::priorityToString(Priority priority)
{
    switch (priority) {
        case Priority::LOW: return "LOW";
        case Priority::HIGH: return "HIGH";
        case Priority::CRITICAL: return "CRITICAL";
        case Priority::NORMAL:
        default: return "NORMAL";
    }
}

} // namespace ReliNet
