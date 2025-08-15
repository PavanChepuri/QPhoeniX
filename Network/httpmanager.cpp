#include "Network/httpmanager.h"
#include "Network/kiteconnectapi.h" // Include for RequestType enum
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>
#include <QVariant> // For storing RequestType property

HttpManager::HttpManager(QObject *parent) : QObject(parent) {
    // Initialize the network manager
    m_networkManager = new QNetworkAccessManager(this);

    // Setup any global network configurations if necessary (e.g., proxy, SSL)
    // Example: m_networkManager->setStrictTransportSecurityEnabled(true);
}

HttpManager::~HttpManager() {
    // m_networkManager is deleted automatically by Qt's parent-child relationship
}

void HttpManager::sendGetRequest(const QNetworkRequest &request, RequestType requestType) {
    qDebug() << "HttpManager::sendGetRequest: url, requestType =" << request.url() << ", " << static_cast<int>(requestType);
    QNetworkReply *reply = m_networkManager->get(request);

    if (reply) {
        // Store the requestType as a custom property on the reply object for later retrieval
        reply->setProperty("requestType", QVariant::fromValue(requestType));

        // Connect the finished signal to our internal slot
        connect(reply, &QNetworkReply::finished, this, &HttpManager::onReplyFinished);

        // Optional: Connect other signals like errorOccurred or sslErrors if needed
        // connect(reply, &QNetworkReply::errorOccurred, this, &HttpManager::handleNetworkError);
    } else {
        qWarning() << "HttpManager: Failed to create GET reply object for URL:" << request.url();
        // Consider emitting an immediate error signal if creation fails
    }
}

void HttpManager::sendPostRequest(const QNetworkRequest &request, const QByteArray &data, RequestType requestType) {
    qDebug() << "HttpManager::sendPostRequest: url, requestType =" << request.url() << ", " << static_cast<int>(requestType);
    QNetworkReply *reply = m_networkManager->post(request, data);

    if (reply) {
        // Store the requestType as a custom property
        reply->setProperty("requestType", QVariant::fromValue(requestType));

        // Connect the finished signal
        connect(reply, &QNetworkReply::finished, this, &HttpManager::onReplyFinished);

        // Optional: Connect other signals
    } else {
        qWarning() << "HttpManager: Failed to create POST reply object for URL:" << request.url();
    }
}

// Slot connected to QNetworkReply::finished()
void HttpManager::onReplyFinished() {
    // Get the reply object that emitted the signal
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qWarning() << "HttpManager::onReplyFinished: Invalid sender object!";
        return;
    }

    // Retrieve the requestType stored earlier as a property
    RequestType type = reply->property("requestType").value<RequestType>();

    qDebug() << "HttpManager::onReplyFinished: Emitting requestFinished signal with requestType =" << static_cast<int>(type);

    // Emit the main signal for the API handler (e.g., KiteConnectAPI) to process
    emit requestFinished(reply, type);

    // IMPORTANT: Do NOT deleteLater() the reply here.
    // The ownership is transferred to the receiver of the requestFinished signal,
    // which is responsible for deleting it after processing.
}
