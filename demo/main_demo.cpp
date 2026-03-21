#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "endpoint_manager.hpp"
#include "retry_engine.hpp"
#include "metrics.hpp"
#include "protocol.hpp"

using namespace ReliNet;

class ReliNetDemo : public QObject {
    Q_OBJECT
    
public:
    ReliNetDemo(QObject* parent = nullptr) : QObject(parent) {
        endpoint_manager_ = new EndpointManager(this);
        metrics_ = new Metrics(this);
        
        // Setup demo scenario
        setupEndpoints();
        runDemoScenario();
    }
    
private slots:
    void runDemoScenario() {
        qDebug() << "=== ReliNet Demo Scenario ===";
        
        // Step 1: Start with SCTP link to primary endpoint
        qDebug() << "1. Starting with SCTP multi-homed connection to primary endpoint";
        simulateConnection("primary", true);
        
        // Step 2: One SCTP path fails - simulate path switch
        QTimer::singleShot(2000, [this]() {
            qDebug() << "2. SCTP path failure - switching within association";
            // Simulate path switch without failover
        });
        
        // Step 3: Both SCTP paths fail - fallback to TCP
        QTimer::singleShot(4000, [this]() {
            qDebug() << "3. All SCTP paths failed - falling back to TCP on same endpoint";
            // This would trigger protocol fallback
        });
        
        // Step 4: TCP fails - endpoint failover to satellite
        QTimer::singleShot(6000, [this]() {
            qDebug() << "4. TCP failed - failing over to satellite endpoint";
            endpoint_manager_->handleTransportError("primary", "TCP connection lost");
            metrics_->incrementFailovers();
        });
        
        // Step 5: SOS message during failover
        QTimer::singleShot(7000, [this]() {
            qDebug() << "5. SOS message inserted - broadcasting to all reachable endpoints";
            sendSOSMessage();
        });
        
        // Step 6: Primary endpoint recovers
        QTimer::singleShot(10000, [this]() {
            qDebug() << "6. Primary SCTP endpoint recovered - restoring as active";
            endpoint_manager_->updateEndpointReachability("primary", true);
        });
        
        // Step 7: Print final stats
        QTimer::singleShot(12000, [this]() {
            printFinalStats();
            QCoreApplication::quit();
        });
    }
    
private:
    void setupEndpoints() {
        // Primary SCTP multi-homed endpoint
        Endpoint primary("primary", "Primary Command Center", 
                        QStringList{"192.168.1.10", "10.0.1.10"}, 
                        8080, TransportType::SCTP, 100, DeploymentContext::MARITIME);
        
        // Secondary TCP endpoint
        Endpoint secondary("secondary", "Secondary Relay", 
                          QStringList{"192.168.1.20"}, 
                          8081, TransportType::TCP, 80, DeploymentContext::MARITIME);
        
        // Satellite backup
        Endpoint satellite("satellite", "Satellite Uplink", 
                          QStringList{"SAT_MODEM_1"}, 
                          0, TransportType::SATELLITE, 60, DeploymentContext::MARITIME);
        
        endpoint_manager_->addEndpoint(primary);
        endpoint_manager_->addEndpoint(secondary);  
        endpoint_manager_->addEndpoint(satellite);
        
        // Connect signals
        connect(endpoint_manager_, &EndpointManager::endpointSwitched, 
                [this](const QString& from, const QString& to, const QString& reason) {
            qDebug() << "Endpoint switched from" << from << "to" << to << "reason:" << reason;
        });
        
        connect(endpoint_manager_, &EndpointManager::allEndpointsUnreachable, 
                []() {
            qDebug() << "CRITICAL: All endpoints unreachable!";
        });
    }
    
    void simulateConnection(const QString& endpoint_id, bool connected) {
        endpoint_manager_->updateEndpointReachability(endpoint_id, connected);
        if (connected) {
            metrics_->incrementMessagesSent();
            qDebug() << "Connected to endpoint:" << endpoint_id;
        }
    }
    
    void sendSOSMessage() {
        // Create SOS message
        Message sos_msg(12345, Priority::SOS, ContextTag::MARITIME, 
                       QByteArray("MAYDAY MAYDAY - Vessel taking on water - Position 45.123N 125.456W"));
        
        qDebug() << "Sending SOS message:" << sos_msg.header.message_id;
        
        // In emergency mode, broadcast to all reachable endpoints
        endpoint_manager_->setEmergencyBroadcastMode(true);
        
        // Mark as sent and eventually acked
        metrics_->incrementMessagesSent();
        QTimer::singleShot(1000, [this]() {
            metrics_->incrementMessagesAcked();
            qDebug() << "SOS message acknowledged by multiple endpoints";
        });
    }
    
    void printFinalStats() {
        qDebug() << "\n=== Final Delivery Statistics ===";
        auto stats = metrics_->getStats();
        qDebug() << "Messages sent:" << stats.messages_sent;
        qDebug() << "Messages ACKed:" << stats.messages_acked;
        qDebug() << "Messages failed:" << stats.messages_failed;
        qDebug() << "Total retries:" << stats.retries_total;
        qDebug() << "Failover count:" << stats.failover_count;
        qDebug() << "Active endpoint:" << stats.active_endpoint;
        qDebug() << "SCTP path switches:" << stats.sctp_path_switches;
        
        // Calculate delivery rate
        float delivery_rate = stats.messages_sent > 0 ? 
            (float)stats.messages_acked / stats.messages_sent * 100.0f : 0.0f;
        qDebug() << "Delivery rate:" << delivery_rate << "%";
    }
    
    EndpointManager* endpoint_manager_;
    Metrics* metrics_;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "ReliNet Guaranteed Message Delivery Demo";
    qDebug() << "Simulating maritime vessel communication scenario\n";
    
    ReliNetDemo demo;
    
    return app.exec();
}

#include "main_demo.moc"