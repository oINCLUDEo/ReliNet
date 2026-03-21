#include "protocol_fallback.hpp"

namespace ReliNet {

ProtocolFallback::ProtocolFallback(QObject* parent) : QObject(parent) {}

void ProtocolFallback::setFallbackChain(const QString& endpoint_id, const QList<TransportType>& chain) {
    TransportFallbackChain fallback;
    fallback.endpoint_id = endpoint_id;
    fallback.transport_order = chain;
    fallback_chains_[endpoint_id] = fallback;
}

TransportType ProtocolFallback::getCurrentTransport(const QString& endpoint_id) {
    auto it = fallback_chains_.find(endpoint_id);
    if (it != fallback_chains_.end() && !it->transport_order.isEmpty()) {
        int index = qMin(it->current_index, it->transport_order.size() - 1);
        return it->transport_order[index];
    }
    return TransportType::TCP; // Default fallback
}

TransportType ProtocolFallback::getNextTransport(const QString& endpoint_id) {
    auto it = fallback_chains_.find(endpoint_id);
    if (it != fallback_chains_.end()) {
        if (it->current_index + 1 < it->transport_order.size()) {
            it->current_index++;
            return it->transport_order[it->current_index];
        } else {
            emit allTransportsExhausted(endpoint_id);
        }
    }
    return TransportType::TCP;
}

void ProtocolFallback::reportTransportFailure(const QString& endpoint_id, const QString& reason) {
    auto it = fallback_chains_.find(endpoint_id);
    if (it != fallback_chains_.end()) {
        TransportType current = getCurrentTransport(endpoint_id);
        TransportType next = getNextTransport(endpoint_id);
        it->failure_count++;
        it->last_error = reason;
        
        if (next != current) {
            emit transportFallbackTriggered(endpoint_id, current, next, reason);
        }
    }
}

void ProtocolFallback::resetToPreferred(const QString& endpoint_id) {
    auto it = fallback_chains_.find(endpoint_id);
    if (it != fallback_chains_.end()) {
        it->current_index = 0;
        it->failure_count = 0;
    }
}

} // namespace ReliNet

#include "protocol_fallback.moc"