#pragma once

#include <QObject>
#include <QString>

class WebBridge : public QObject {
    Q_OBJECT

public:
    explicit WebBridge(QObject* parent = nullptr);
    ~WebBridge() override = default;

public slots:
    void logMessage(const QString& msg);
    void updateTOC(const QString& tocJson);
    void setActiveHeading(const QString& headingId);

signals:
    void tocUpdated(const QString& tocJson);
    void activeHeadingChanged(const QString& headingId);
};
