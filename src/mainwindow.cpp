#include "mainwindow.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QApplication>

namespace ReliNet {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUI();
    setDeploymentMode(DeploymentMode::GENERAL);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    central_widget_ = new QWidget;
    setCentralWidget(central_widget_);
    
    auto* main_layout = new QVBoxLayout(central_widget_);
    
    setupLinkStatusPanel();
    setupEndpointRegistryPanel();
    setupMessageQueuePanel();
    setupMetricsPanel();
    setupEventLogPanel();
    setupTestMessagePanel();
    
    resize(1200, 800);
}

void MainWindow::setupLinkStatusPanel() {
    auto* group = new QGroupBox("Link Status");
    auto* layout = new QGridLayout(group);
    
    link_status_label_ = new QLabel("Disconnected");
    endpoint_label_ = new QLabel("None");
    transport_label_ = new QLabel("None");
    link_quality_bar_ = new QProgressBar;
    rtt_label_ = new QLabel("0 ms");
    sctp_paths_label_ = new QLabel("0 paths");
    
    layout->addWidget(new QLabel("Status:"), 0, 0);
    layout->addWidget(link_status_label_, 0, 1);
    layout->addWidget(new QLabel("Endpoint:"), 1, 0);
    layout->addWidget(endpoint_label_, 1, 1);
    layout->addWidget(new QLabel("Transport:"), 2, 0);
    layout->addWidget(transport_label_, 2, 1);
    layout->addWidget(new QLabel("Quality:"), 0, 2);
    layout->addWidget(link_quality_bar_, 0, 3);
    layout->addWidget(new QLabel("RTT:"), 1, 2);
    layout->addWidget(rtt_label_, 1, 3);
    layout->addWidget(new QLabel("SCTP Paths:"), 2, 2);
    layout->addWidget(sctp_paths_label_, 2, 3);
    
    static_cast<QVBoxLayout*>(central_widget_->layout())->addWidget(group);
}

void MainWindow::setupEndpointRegistryPanel() {
    auto* group = new QGroupBox("Endpoint Registry");
    auto* layout = new QVBoxLayout(group);
    
    endpoint_table_ = new QTableWidget(0, 5);
    endpoint_table_->setHorizontalHeaderLabels({"Status", "Name", "Transport", "Priority", "Context"});
    endpoint_table_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(endpoint_table_);
    
    static_cast<QVBoxLayout*>(central_widget_->layout())->addWidget(group);
}

void MainWindow::setupMessageQueuePanel() {
    auto* group = new QGroupBox("Message Queue");
    auto* layout = new QVBoxLayout(group);
    
    message_queue_table_ = new QTableWidget(0, 6);
    message_queue_table_->setHorizontalHeaderLabels({"ID", "Priority", "Status", "Retries", "Transport", "Destination"});
    message_queue_table_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(message_queue_table_);
    
    static_cast<QVBoxLayout*>(central_widget_->layout())->addWidget(group);
}

void MainWindow::setupMetricsPanel() {
    auto* group = new QGroupBox("Live Metrics");
    auto* layout = new QGridLayout(group);
    
    sent_label_ = new QLabel("0");
    acked_label_ = new QLabel("0");
    failed_label_ = new QLabel("0");
    retries_label_ = new QLabel("0");
    queue_depth_label_ = new QLabel("0");
    failover_count_label_ = new QLabel("0");
    
    layout->addWidget(new QLabel("Sent:"), 0, 0);
    layout->addWidget(sent_label_, 0, 1);
    layout->addWidget(new QLabel("ACKed:"), 0, 2);
    layout->addWidget(acked_label_, 0, 3);
    layout->addWidget(new QLabel("Failed:"), 0, 4);
    layout->addWidget(failed_label_, 0, 5);
    layout->addWidget(new QLabel("Retries:"), 1, 0);
    layout->addWidget(retries_label_, 1, 1);
    layout->addWidget(new QLabel("Queue:"), 1, 2);
    layout->addWidget(queue_depth_label_, 1, 3);
    layout->addWidget(new QLabel("Failovers:"), 1, 4);
    layout->addWidget(failover_count_label_, 1, 5);
    
    static_cast<QVBoxLayout*>(central_widget_->layout())->addWidget(group);
}

void MainWindow::setupEventLogPanel() {
    auto* group = new QGroupBox("Event Log");
    auto* layout = new QVBoxLayout(group);
    
    event_log_ = new QTextEdit;
    event_log_->setMaximumHeight(150);
    event_log_->setReadOnly(true);
    layout->addWidget(event_log_);
    
    static_cast<QVBoxLayout*>(central_widget_->layout())->addWidget(group);
}

void MainWindow::setupTestMessagePanel() {
    auto* group = new QGroupBox("Send Test Message");
    auto* layout = new QHBoxLayout(group);
    
    test_message_input_ = new QLineEdit;
    test_message_input_->setPlaceholderText("Enter test message...");
    
    priority_selector_ = new QComboBox;
    priority_selector_->addItems({"ROUTINE", "URGENT", "DISTRESS", "SOS"});
    
    destination_selector_ = new QComboBox;
    destination_selector_->addItem("Primary");
    
    send_button_ = new QPushButton("Send");
    connect(send_button_, &QPushButton::clicked, this, &MainWindow::onSendTestButtonClicked);
    
    layout->addWidget(new QLabel("Message:"));
    layout->addWidget(test_message_input_);
    layout->addWidget(new QLabel("Priority:"));
    layout->addWidget(priority_selector_);
    layout->addWidget(new QLabel("Destination:"));
    layout->addWidget(destination_selector_);
    layout->addWidget(send_button_);
    
    static_cast<QVBoxLayout*>(central_widget_->layout())->addWidget(group);
}

void MainWindow::setDeploymentMode(DeploymentMode mode) {
    current_mode_ = mode;
    
    QPalette palette = QApplication::palette();
    
    switch (mode) {
    case DeploymentMode::MARITIME:
        setWindowTitle("ReliNet - Maritime Communications");
        // Navy/slate color scheme would go here
        break;
    case DeploymentMode::MILITARY:
        setWindowTitle("ReliNet - Military Communications");
        // Olive/charcoal color scheme would go here
        break;
    case DeploymentMode::GENERAL:
        setWindowTitle("ReliNet - General Communications");
        // Default color scheme
        break;
    }
    
    setPalette(palette);
}

void MainWindow::onSendTestButtonClicked() {
    QString message = test_message_input_->text();
    int priority = priority_selector_->currentIndex();
    QString destination = destination_selector_->currentText();
    
    if (!message.isEmpty()) {
        emit sendTestMessage(message, priority, destination);
        test_message_input_->clear();
    }
}

void MainWindow::onDeploymentModeChanged() {
    // Implementation for mode switching
}

} // namespace ReliNet

#include "mainwindow.moc"