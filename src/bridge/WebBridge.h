#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>

class WebBridge : public QObject {
    Q_OBJECT

public:
    explicit WebBridge(QObject* parent = nullptr);
    ~WebBridge() override = default;

public slots:
    // Uplink Slots (Invoked by Web runtime -> Emitted as C++ signals)
    void bridgeReady();
    void onActiveHeadingChanged(const QString& headingId);
    void onDocumentRendered(int headingCount);
    void logMessage(const QString& msg);
    void updateTOC(const QString& tocJson);
    void setActiveHeading(const QString& headingId);

signals:
    // Downlink Signals (Dispatched by C++ -> Received by Web runtime)
    void renderDocument(const QString& markdown, const QJsonObject& options = QJsonObject());
    void navigateToHeading(const QString& headingId);
    void setTheme(const QString& themeName);

    // Uplink Signals (Dispatched to C++ listeners)
    void bridgeInitialized();
    void activeHeadingChanged(const QString& headingId);
    void documentRendered(int headingCount);
    void tocUpdated(const QString& tocJson);
};
