#ifndef ICHANNEL_H
#define ICHANNEL_H

#include "Message.h"
#include <QObject>

namespace ReliNet {

class IChannel : public QObject {
    Q_OBJECT

public:
    explicit IChannel(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IChannel() = default;

    virtual void send(const Message& msg) = 0;
    virtual bool isAvailable() const = 0;
    virtual QString name() const = 0;

signals:
    void messageSent(const QString& messageId);
    void sendFailed(const QString& messageId, const QString& error);
    void ackReceived(const QString& messageId);
};

} // namespace ReliNet

#endif // ICHANNEL_H
