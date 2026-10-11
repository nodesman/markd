#include "WebBridge.h"
#include <QDebug>

WebBridge::WebBridge(QObject* parent)
    : QObject(parent)
{
}

void WebBridge::logMessage(const QString& msg)
{
    qDebug() << "[WebBridge]" << msg;
}

void WebBridge::updateTOC(const QString& tocJson)
{
    emit tocUpdated(tocJson);
}

void WebBridge::setActiveHeading(const QString& headingId)
{
    emit activeHeadingChanged(headingId);
}
