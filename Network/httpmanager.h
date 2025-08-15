#ifndef HTTPMANAGER_H
#define HTTPMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

// Include header where RequestType is defined (kiteconnectapi.h)
#include "Network/kiteconnectapi.h" // Adjust path as needed

/**
 * @brief Manages network requests (GET/POST) for the application.
 *
 * Handles sending requests via QNetworkAccessManager and reports results
 * back via signals, associating a RequestType with each reply.
 */
class HttpManager : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor.
     * @param parent Optional parent QObject.
     */
    explicit HttpManager(QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~HttpManager();

    /**
     * @brief Sends an asynchronous GET request.
     * @param request The QNetworkRequest object containing URL, headers, etc.
     * @param requestType The type of request being sent (for context tracking).
     */
    void sendGetRequest(const QNetworkRequest &request, RequestType requestType);

    /**
     * @brief Sends an asynchronous POST request.
     * @param request The QNetworkRequest object containing URL, headers, etc.
     * @param data The data payload to be sent with the POST request.
     * @param requestType The type of request being sent (for context tracking).
     */
    void sendPostRequest(const QNetworkRequest &request, const QByteArray &data, RequestType requestType);

signals:
    /**
     * @brief Emitted when any network request finishes (successfully or with error).
     * @param reply Pointer to the completed QNetworkReply object. The receiver is responsible for calling deleteLater().
     * @param type The RequestType associated with this reply.
     */
    void requestFinished(QNetworkReply *reply, RequestType type);

private slots:
    /**
     * @brief Slot connected to the finished() signal of QNetworkReply.
     * Retrieves the RequestType property and emits the requestFinished signal.
     */
    void onReplyFinished();

private:
    QNetworkAccessManager *m_networkManager; // Manages network access.
};

#endif // HTTPMANAGER_H
