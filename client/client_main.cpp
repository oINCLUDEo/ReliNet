#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include "../lib/include/Message.h"
#include "../lib/include/MessageManager.h"
#include "../lib/include/PersistentQueue.h"
#include "../lib/include/DeliveryEngine.h"
#include "../lib/include/ChannelManager.h"
#include "../lib/include/PrimaryChannel.h"
#include "../lib/include/BackupChannel.h"

using namespace ReliNet;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "=== ReliNet Client Demo ===";

    // Initialize components
    PersistentQueue queue("relinet_client.db");
    if (!queue.initialize()) {
        qDebug() << "Failed to initialize queue";
        return 1;
    }

    // Setup channels
    ChannelManager channelManager;

    PrimaryChannel* primaryChannel = new PrimaryChannel("127.0.0.1", 12345);
    BackupChannel* backupChannel = new BackupChannel("127.0.0.1", 12346);

    channelManager.addChannel(primaryChannel);
    channelManager.addChannel(backupChannel);

    // Connect channels
    primaryChannel->connectToServer();
    backupChannel->connectToServer();

    // Setup delivery engine
    DeliveryEngine engine(&queue, &channelManager);
    engine.setAckTimeout(3000);      // 3 seconds
    engine.setMaxRetries(5);
    engine.setProcessInterval(2000); // Check queue every 2 seconds

    // Setup message manager
    MessageManager messageManager(&queue);

    // Connect signals for logging
    QObject::connect(&engine, &DeliveryEngine::messageDelivered, [](const QString& id) {
        qDebug() << "✓ Message delivered:" << id;
    });

    QObject::connect(&engine, &DeliveryEngine::messageFailed, [](const QString& id, const QString& reason) {
        qDebug() << "✗ Message failed:" << id << "-" << reason;
    });

    QObject::connect(&engine, &DeliveryEngine::retryingMessage, [](const QString& id, int retry) {
        qDebug() << "↻ Retrying message:" << id << "- attempt" << retry;
    });

    // Start the delivery engine after a short delay to allow connections
    QTimer::singleShot(1000, [&engine]() {
        engine.start();
    });

    // Send test messages with different priorities
    QTimer::singleShot(2000, [&messageManager]() {
        qDebug() << "\n--- Sending test messages ---";

        messageManager.sendMessage("Low priority message", Priority::LOW);
        messageManager.sendMessage("Normal priority message", Priority::NORMAL);
        messageManager.sendMessage("High priority message", Priority::HIGH);
        messageManager.sendMessage("Critical priority message", Priority::CRITICAL);
    });

    // Send additional messages every 10 seconds
    QTimer* sendTimer = new QTimer(&app);
    QObject::connect(sendTimer, &QTimer::timeout, [&messageManager]() {
        static int counter = 1;
        QString message = QString("Periodic message #%1").arg(counter++);
        messageManager.sendMessage(message.toUtf8(), Priority::NORMAL);
        qDebug() << "\n--- Sent periodic message ---";
    });
    QTimer::singleShot(15000, sendTimer, [sendTimer]() { sendTimer->start(10000); });

    // Optional: Simulate disconnect/reconnect for testing
    /*
    QTimer::singleShot(20000, primaryChannel, [primaryChannel]() {
        qDebug() << "\n=== Simulating primary channel disconnect ===";
        primaryChannel->disconnectFromServer();
    });

    QTimer::singleShot(30000, primaryChannel, [primaryChannel]() {
        qDebug() << "\n=== Reconnecting primary channel ===";
        primaryChannel->connectToServer();
    });
    */

    qDebug() << "\nClient is running. Press Ctrl+C to exit.";
    qDebug() << "Connecting to servers...";

    return app.exec();
}
