#ifndef CHANNELMANAGER_H
#define CHANNELMANAGER_H

#include "IChannel.h"
#include <QObject>
#include <QList>

namespace ReliNet {

class ChannelManager : public QObject {
    Q_OBJECT

public:
    explicit ChannelManager(QObject* parent = nullptr);
    ~ChannelManager() override;

    void addChannel(IChannel* channel);
    IChannel* getPrimaryChannel() const;
    IChannel* getBackupChannel() const;
    QList<IChannel*> getAllChannels() const;
    QList<IChannel*> getAvailableChannels() const;

signals:
    void channelAvailable(IChannel* channel);
    void channelUnavailable(IChannel* channel);

private:
    QList<IChannel*> m_channels;
};

} // namespace ReliNet

#endif // CHANNELMANAGER_H
