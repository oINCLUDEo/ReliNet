#pragma once

#include "protocol.hpp"
#include "itransport.hpp"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QMap>
#include <QDateTime>
#include <memory>

namespace ReliNet {

enum class DeploymentContext {
    MARITIME,
    MILITARY,
    GENERAL
};

struct Endpoint {
    QString id;
    QString name;
    QStringList addresses; // Multi-homing support for SCTP
    uint16_t port;
    TransportType preferred_transport;
    int priority; // Higher = more preferred
    DeploymentContext context;
    bool reachable = false;
    QDateTime last_seen;
    int failure_count = 0;
    
    Endpoint() = default;
    Endpoint(const QString& endpoint_id, const QString& endpoint_name, 
             const QStringList& addr_list, uint16_t p, TransportType transport, 
             int prio, DeploymentContext ctx);
};

enum class FailoverReason {
    HeartbeatFailure,
    AckTimeouts,
    TransportError,
    ManualSwitch,
    EmergencyBroadcast
};

class EndpointManager : public QObject {
    Q_OBJECT
    
public:
    explicit EndpointManager(QObject* parent = nullptr);
    ~EndpointManager();
    
    // Endpoint management
    void addEndpoint(const Endpoint& endpoint);
    void removeEndpoint(const QString& endpoint_id);
    void updateEndpointReachability(const QString& endpoint_id, bool reachable);
    
    // Active endpoint management
    QString getActiveEndpointId() const { return active_endpoint_id_; }
    Endpoint* getActiveEndpoint();
    const Endpoint* getActiveEndpoint() const;
    
    // Failover control
    void triggerFailover(const QString& reason = "Manual");
    void setEmergencyBroadcastMode(bool enabled);
    bool isEmergencyBroadcastActive() const { return emergency_broadcast_active_; }
    
    // Recovery and monitoring
    void startRecoveryProbe(int interval_seconds = 60);
    void stopRecoveryProbe();
    
    // Statistics
    QStringList getReachableEndpointIds() const;
    QList<Endpoint> getAllEndpoints() const;
    int getFailoverCount() const { return failover_count_; }
    
signals:
    void endpointAdded(const QString& endpoint_id);
    void endpointRemoved(const QString& endpoint_id);
    void endpointSwitched(const QString& from_id, const QString& to_id, const QString& reason);
    void allEndpointsUnreachable();
    void endpointRecovered(const QString& endpoint_id);
    void emergencyBroadcastInitiated(uint64_t message_id);
    void endpointReachabilityChanged(const QString& endpoint_id, bool reachable);
    
public slots:
    void handleHeartbeatFailure(const QString& endpoint_id);
    void handleAckTimeouts(const QString& endpoint_id, int consecutive_timeouts);
    void handleTransportError(const QString& endpoint_id, const QString& error);
    
private slots:
    void onRecoveryProbeTimer();
    
private:
    Endpoint* findBestEndpoint();
    void switchToEndpoint(const QString& endpoint_id, const QString& reason);
    void markEndpointUnreachable(const QString& endpoint_id, const QString& reason);
    void markEndpointReachable(const QString& endpoint_id);
    
    QMap<QString, Endpoint> endpoints_;
    QString active_endpoint_id_;
    bool emergency_broadcast_active_ = false;
    
    QTimer* recovery_probe_timer_;
    int failover_count_ = 0;
    int recovery_interval_seconds_ = 60;
    
    // Failure tracking
    QMap<QString, int> consecutive_ack_timeouts_;
    static constexpr int MAX_ACK_TIMEOUTS = 3;
};

} // namespace ReliNet