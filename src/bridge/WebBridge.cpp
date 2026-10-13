#include "bridge/WebBridge.h"
#include <QDebug>

WebBridge::WebBridge(QObject* parent)
    : QObject(parent)
{
}

void WebBridge::logMessage(const QString& msg)
{
    qDebug() << "[WebBridge]" << msg;
}

void WebBridge::bridgeReady()
{
    qDebug() << "[WebBridge] Handshake received: bridgeReady";
    emit bridgeInitialized();
}

void WebBridge::onActiveHeadingChanged(const QString& headingId)
{
    emit activeHeadingChanged(headingId);
}

void WebBridge::onDocumentRendered(int headingCount)
{
    emit documentRendered(headingCount);
}

void WebBridge::updateTOC(const QString& tocJson)
{
    emit tocUpdated(tocJson);
}

void WebBridge::setActiveHeading(const QString& headingId)
{
    onActiveHeadingChanged(headingId);
}
