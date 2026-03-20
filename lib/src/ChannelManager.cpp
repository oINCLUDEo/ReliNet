#include "ChannelManager.h"

namespace ReliNet {

ChannelManager::ChannelManager(QObject* parent)
    : QObject(parent)
{
}

ChannelManager::~ChannelManager()
{
}

void ChannelManager::addChannel(IChannel* channel)
{
    if (channel && !m_channels.contains(channel)) {
        m_channels.append(channel);
        channel->setParent(this);
    }
}

IChannel* ChannelManager::getPrimaryChannel() const
{
    return m_channels.isEmpty() ? nullptr : m_channels.first();
}

IChannel* ChannelManager::getBackupChannel() const
{
    return m_channels.size() > 1 ? m_channels.at(1) : nullptr;
}

QList<IChannel*> ChannelManager::getAllChannels() const
{
    return m_channels;
}

QList<IChannel*> ChannelManager::getAvailableChannels() const
{
    QList<IChannel*> available;
    for (IChannel* channel : m_channels) {
        if (channel->isAvailable()) {
            available.append(channel);
        }
    }
    return available;
}

} // namespace ReliNet
