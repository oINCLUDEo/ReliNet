#pragma once

#include "itransport.hpp"
#include <QObject>
#include <QMap>
#include <QList>

namespace ReliNet {

struct TransportFallbackChain {
    QString endpoint_id;
    QList<TransportType> transport_order;
    int current_index = 0;
    int failure_count = 0;
    QString last_error;
};

class ProtocolFallback : public QObject {
    Q_OBJECT
    
public:
    explicit ProtocolFallback(QObject* parent = nullptr);
    
    void setFallbackChain(const QString& endpoint_id, const QList<TransportType>& chain);
    TransportType getCurrentTransport(const QString& endpoint_id);
    TransportType getNextTransport(const QString& endpoint_id);
    void reportTransportFailure(const QString& endpoint_id, const QString& reason);
    void resetToPreferred(const QString& endpoint_id);
    
signals:
    void transportFallbackTriggered(const QString& endpoint_id, TransportType from, TransportType to, const QString& reason);
    void allTransportsExhausted(const QString& endpoint_id);
    
private:
    QMap<QString, TransportFallbackChain> fallback_chains_;
};

} // namespace ReliNet