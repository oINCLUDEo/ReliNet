#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>

namespace ReliNet {

enum class DeploymentMode {
    MARITIME,
    MILITARY,
    GENERAL
};

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    void setDeploymentMode(DeploymentMode mode);
    
signals:
    void sendTestMessage(const QString& text, int priority, const QString& destination);
    
private slots:
    void onSendTestButtonClicked();
    void onDeploymentModeChanged();
    
private:
    void setupUI();
    void setupLinkStatusPanel();
    void setupEndpointRegistryPanel();
    void setupMessageQueuePanel();
    void setupMetricsPanel();
    void setupEventLogPanel();
    void setupTestMessagePanel();
    
    DeploymentMode current_mode_ = DeploymentMode::GENERAL;
    
    // UI Components
    QWidget* central_widget_;
    
    // Link Status
    QLabel* link_status_label_;
    QLabel* endpoint_label_;
    QLabel* transport_label_;
    QProgressBar* link_quality_bar_;
    QLabel* rtt_label_;
    QLabel* sctp_paths_label_;
    
    // Endpoint Registry
    QTableWidget* endpoint_table_;
    
    // Message Queue
    QTableWidget* message_queue_table_;
    
    // Metrics
    QLabel* sent_label_;
    QLabel* acked_label_;
    QLabel* failed_label_;
    QLabel* retries_label_;
    QLabel* queue_depth_label_;
    QLabel* failover_count_label_;
    
    // Event Log
    QTextEdit* event_log_;
    
    // Test Message
    QLineEdit* test_message_input_;
    QComboBox* priority_selector_;
    QComboBox* destination_selector_;
    QPushButton* send_button_;
};

} // namespace ReliNet