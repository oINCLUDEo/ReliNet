#include "endpoint_manager.hpp"
#include <QDebug>
#include <algorithm>

namespace ReliNet {

Endpoint::Endpoint(const QString& endpoint_id, const QString& endpoint_name,
                  const QStringList& addr_list, uint16_t p, TransportType transport,
                  int prio, DeploymentContext ctx)
    : id(endpoint_id), name(endpoint_name), addresses(addr_list), port(p),
      preferred_transport(transport), priority(prio), context(ctx), reachable(false) {
    last_seen = QDateTime::currentDateTime();
}

EndpointManager::EndpointManager(QObject* parent) : QObject(parent) {
    recovery_probe_timer_ = new QTimer(this);
    connect(recovery_probe_timer_, &QTimer::timeout, this, &EndpointManager::onRecoveryProbeTimer);
}

EndpointManager::~EndpointManager() = default;

void EndpointManager::addEndpoint(const Endpoint& endpoint) {
    endpoints_[endpoint.id] = endpoint;
    emit endpointAdded(endpoint.id);
    
    // If no active endpoint, make this one active
    if (active_endpoint_id_.isEmpty() || endpoint.priority > getActiveEndpoint()->priority) {
        switchToEndpoint(endpoint.id, "Higher priority endpoint added");
    }
}

void EndpointManager::removeEndpoint(const QString& endpoint_id) {
    auto it = endpoints_.find(endpoint_id);
    if (it == endpoints_.end()) return;
    
    // If removing active endpoint, trigger failover first
    if (endpoint_id == active_endpoint_id_) {
        triggerFailover("Active endpoint removed");
    }
    
    endpoints_.erase(it);
    consecutive_ack_timeouts_.remove(endpoint_id);
    emit endpointRemoved(endpoint_id);
}

void EndpointManager::updateEndpointReachability(const QString& endpoint_id, bool reachable) {
    auto it = endpoints_.find(endpoint_id);
    if (it == endpoints_.end()) return;
    
    bool was_reachable = it->reachable;
    it->reachable = reachable;
    it->last_seen = QDateTime::currentDateTime();
    
    if (reachable && !was_reachable) {
        it->failure_count = 0;
        consecutive_ack_timeouts_[endpoint_id] = 0;
        markEndpointReachable(endpoint_id);
    } else if (!reachable && was_reachable) {
        markEndpointUnreachable(endpoint_id, "Connectivity lost");
    }
    
    emit endpointReachabilityChanged(endpoint_id, reachable);
}

Endpoint* EndpointManager::getActiveEndpoint() {
    auto it = endpoints_.find(active_endpoint_id_);
    return (it != endpoints_.end()) ? &it.value() : nullptr;
}

const Endpoint* EndpointManager::getActiveEndpoint() const {
    auto it = endpoints_.find(active_endpoint_id_);
    return (it != endpoints_.end()) ? &it.value() : nullptr;
}

void EndpointManager::triggerFailover(const QString& reason) {
    QString old_endpoint = active_endpoint_id_;
    
    // Mark current endpoint as unreachable if it exists
    if (!active_endpoint_id_.isEmpty()) {
        markEndpointUnreachable(active_endpoint_id_, reason);
    }
    
    // Find best alternative endpoint
    Endpoint* best = findBestEndpoint();
    if (best) {
        switchToEndpoint(best->id, reason);
    } else {
        active_endpoint_id_.clear();
        emit allEndpointsUnreachable();
    }
}

void EndpointManager::setEmergencyBroadcastMode(bool enabled) {
    emergency_broadcast_active_ = enabled;
    if (enabled) {
        // In emergency mode, try to reach all endpoints simultaneously
        for (auto& endpoint : endpoints_) {
            if (endpoint.reachable) {
                // Signal that this endpoint should be used for broadcast
                // Implementation would be handled by higher-level components
            }
        }
    }
}

void EndpointManager::startRecoveryProbe(int interval_seconds) {
    recovery_interval_seconds_ = interval_seconds;
    recovery_probe_timer_->start(interval_seconds * 1000);
}

void EndpointManager::stopRecoveryProbe() {
    recovery_probe_timer_->stop();
}

QStringList EndpointManager::getReachableEndpointIds() const {
    QStringList reachable;
    for (auto it = endpoints_.begin(); it != endpoints_.end(); ++it) {
        if (it.value().reachable) {
            reachable.append(it.key());
        }
    }
    return reachable;
}

QList<Endpoint> EndpointManager::getAllEndpoints() const {
    QList<Endpoint> list;
    for (auto it = endpoints_.begin(); it != endpoints_.end(); ++it) {
        list.append(it.value());
    }
    return list;
}

void EndpointManager::handleHeartbeatFailure(const QString& endpoint_id) {
    auto it = endpoints_.find(endpoint_id);
    if (it == endpoints_.end()) return;
    
    it->failure_count++;
    
    // For active endpoint, trigger immediate failover
    if (endpoint_id == active_endpoint_id_) {
        triggerFailover("Heartbeat failure");
    } else {
        // For non-active endpoints, mark unreachable after repeated failures
        if (it->failure_count >= 3) {
            markEndpointUnreachable(endpoint_id, "Repeated heartbeat failures");
        }
    }
}

void EndpointManager::handleAckTimeouts(const QString& endpoint_id, int consecutive_timeouts) {
    consecutive_ack_timeouts_[endpoint_id] = consecutive_timeouts;
    
    // Trigger failover after threshold
    if (consecutive_timeouts >= MAX_ACK_TIMEOUTS && endpoint_id == active_endpoint_id_) {
        triggerFailover("Consecutive ACK timeouts");
    }
}

void EndpointManager::handleTransportError(const QString& endpoint_id, const QString& error) {
    Q_UNUSED(error)
    
    auto it = endpoints_.find(endpoint_id);
    if (it == endpoints_.end()) return;
    
    it->failure_count++;
    
    // For active endpoint, trigger failover
    if (endpoint_id == active_endpoint_id_) {
        triggerFailover("Transport error: " + error);
    } else {
        markEndpointUnreachable(endpoint_id, "Transport error");
    }
}

void EndpointManager::onRecoveryProbeTimer() {
    // Probe unreachable endpoints to see if they've recovered
    for (auto& endpoint : endpoints_) {
        if (!endpoint.reachable) {
            // In a real implementation, this would initiate a connection test
            // For now, we just emit a signal that could be handled by transport layer
            // to attempt a connection probe
            qDebug() << "Probing endpoint for recovery:" << endpoint.id;
            
            // Simulate recovery check - in real implementation, this would be async
            // and the result would come back via updateEndpointReachability()
        }
    }
}

Endpoint* EndpointManager::findBestEndpoint() {
    Endpoint* best = nullptr;
    
    // Find reachable endpoint with highest priority
    for (auto& endpoint : endpoints_) {
        if (endpoint.reachable) {
            if (!best || endpoint.priority > best->priority) {
                best = &endpoint;
            }
        }
    }
    
    return best;
}

void EndpointManager::switchToEndpoint(const QString& endpoint_id, const QString& reason) {
    QString old_endpoint = active_endpoint_id_;
    active_endpoint_id_ = endpoint_id;
    
    auto it = endpoints_.find(endpoint_id);
    if (it != endpoints_.end()) {
        it->reachable = true; // Assume reachable when we switch to it
        it->failure_count = 0;
        consecutive_ack_timeouts_[endpoint_id] = 0;
    }
    
    if (old_endpoint != endpoint_id) {
        failover_count_++;
        emit endpointSwitched(old_endpoint, endpoint_id, reason);
    }
}

void EndpointManager::markEndpointUnreachable(const QString& endpoint_id, const QString& reason) {
    Q_UNUSED(reason)
    
    auto it = endpoints_.find(endpoint_id);
    if (it != endpoints_.end()) {
        it->reachable = false;
        it->failure_count++;
    }
    
    // If this was the active endpoint, find alternative
    if (endpoint_id == active_endpoint_id_) {
        active_endpoint_id_.clear();
        
        Endpoint* alternative = findBestEndpoint();
        if (alternative) {
            switchToEndpoint(alternative->id, "Failover from unreachable endpoint");
        }
    }
}

void EndpointManager::markEndpointReachable(const QString& endpoint_id) {
    auto it = endpoints_.find(endpoint_id);
    if (it != endpoints_.end()) {
        it->reachable = true;
        it->failure_count = 0;
        emit endpointRecovered(endpoint_id);
        
        // If this endpoint has higher priority than current active, switch to it
        const Endpoint* current_active = getActiveEndpoint();
        if (!current_active || it->priority > current_active->priority) {
            switchToEndpoint(endpoint_id, "Higher priority endpoint recovered");
        }
    }
}

} // namespace ReliNet

#include "endpoint_manager.moc"