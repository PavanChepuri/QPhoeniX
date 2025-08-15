#include "Network/kiteconnectapi.h"
#include "Network/httpmanager.h"
#include "Utils/configurationmanager.h"
#include "Data/datamanager.h" // *** ADDED *** Include DataManager to check instrument segment
#include "Data/DataStructures/InstrumentData.h" // *** ADDED *** Include InstrumentData definition

#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QMessageAuthenticationCode> // For SHA256 Checksum
#include <QCryptographicHash>         // Required for hash algorithm enum
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QMetaType> // For qRegisterMetaType

// Register RequestType enum with the meta-object system for QVariant property storage
int requestTypeMetaTypeId = qRegisterMetaType<RequestType>("RequestType");


// Constructor: Initializes members, fetches API secret.
KiteConnectAPI::KiteConnectAPI(const QString& apiKey, QObject *parent)
    : QObject(parent), m_apiKey(apiKey)
{
    // Fetch API Secret from ConfigurationManager Singleton
    ConfigurationManager* config = ConfigurationManager::instance();
    if (config) {
        m_apiSecret = config->getApiSecret();
    }

    if (m_apiSecret.isEmpty()) {
        qCritical() << "KiteConnectAPI: Failed to retrieve API Secret from ConfigurationManager! Check config file.";
        // Consider setting an internal error state
    }

    // Create the HTTP manager instance
    m_httpManager = new HttpManager(this);

    // Connect the HttpManager's signal to our central reply handler slot
    connect(m_httpManager, &HttpManager::requestFinished, this, &KiteConnectAPI::onNetworkReply);
}

// Destructor
KiteConnectAPI::~KiteConnectAPI() {
    // m_httpManager is deleted by Qt's parent-child mechanism
}

// Accessor for the access token
QString KiteConnectAPI::getAccessToken() const {
    return m_accessToken;
}

// Accessor for the API key
QString KiteConnectAPI::getApiKey() const {
    return m_apiKey;
}


// --- Request Methods ---

// Initiates the web login flow
void KiteConnectAPI::login() {
    QUrl loginRedirectUrl(m_loginUrl);
    QUrlQuery query;
    query.addQueryItem("api_key", m_apiKey); // Use stored member apiKey
    query.addQueryItem("v", m_apiVersion);
    loginRedirectUrl.setQuery(query);
    qDebug() << "KiteConnectAPI::login: Redirecting to URL:" << loginRedirectUrl.toString();
    // Emit signal for MainWindow to show the LoginDialog
    emit requiresUserLoginRedirect(loginRedirectUrl);
}

// Exchanges request token for access token
void KiteConnectAPI::generateSession(const QString& requestToken) {
    if (m_apiKey.isEmpty() || m_apiSecret.isEmpty()) {
        qWarning() << "KiteConnectAPI::generateSession: API Key or Secret is empty (check config).";
        emit sessionGenerationFailed("API Key or Secret not available.");
        return;
    }

    qDebug() << "generateSession: API Key =" << m_apiKey.left(4) << "..."; // Mask key
    qDebug() << "generateSession: Request Token =" << requestToken.left(4) << "..."; // Mask token

    // Calculate SHA256 Checksum: api_key + request_token + api_secret
    QString checksumData = m_apiKey + requestToken + m_apiSecret;
    QString checksum = QCryptographicHash::hash(checksumData.toUtf8(), QCryptographicHash::Sha256).toHex();
    qDebug() << "generateSession: Calculated Checksum =" << checksum;


    QUrl url(m_baseUrl + "/session/token");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("X-Kite-Version", m_apiVersion.toUtf8()); // Set API version header

    // Prepare POST data
    QUrlQuery params;
    params.addQueryItem("api_key", m_apiKey);
    params.addQueryItem("request_token", requestToken);
    params.addQueryItem("checksum", checksum);

    QByteArray postData = params.toString(QUrl::FullyEncoded).toUtf8();
    qDebug() << "generateSession: Payload =" << QString(postData); // Log payload

    // Send POST request via HttpManager
    m_httpManager->sendPostRequest(request, postData, RequestType::SessionRequest);
}

// Fetches the full instrument list (CSV format)
void KiteConnectAPI::fetchAllInstruments() {
    if (m_accessToken.isEmpty()) {
        qWarning() << "KiteConnectAPI::fetchAllInstruments: Access token not available.";
        emit instrumentsFetchFailed("Access token not available.");
        return;
    }
    qDebug() << "KiteConnectAPI::fetchAllInstruments() called!";
    QUrl url(m_baseUrl + "/instruments");
    // Use helper to add Authorization and Version headers
    QNetworkRequest request = createBaseRequest(url);
    m_httpManager->sendGetRequest(request, RequestType::InstrumentsRequest);
}

// Fetches historical candle data
// *** MODIFIED: fetchHistoricalData now conditionally adds 'continuous' parameter ***
void KiteConnectAPI::fetchHistoricalData(const QString& instrumentToken, const QString& interval, const QString& from, const QString& to) {
    if (m_accessToken.isEmpty()) {
        qWarning() << "KiteConnectAPI::fetchHistoricalData: Access token not available for token" << instrumentToken;
        emit historicalDataFailed("Access token not available.", instrumentToken + "_" + interval);
        return;
    }
    qDebug() << "KiteConnectAPI::fetchHistoricalData called for Token:" << instrumentToken << "Interval:" << interval;

    // Construct URL with path parameters
    QUrl url(QString("%1/instruments/historical/%2/%3").arg(m_baseUrl).arg(instrumentToken).arg(interval));

    // Add mandatory query parameters
    QUrlQuery query;
    query.addQueryItem("from", from);
    query.addQueryItem("to", to);
    // query.addQueryItem("oi", "0"); // Add if OI data is needed

    // --- Conditionally add continuous parameter ---
    bool addContinuous = false;
    DataManager* dm = DataManager::instance(); // Get DataManager instance
    if (dm) {
        // Ensure DataManager::getInstrument is public and works
        InstrumentData instrument = dm->getInstrument(instrumentToken);
        // Add continuous=1 ONLY for "day" interval AND NFO-FUT/NFO-OPT segments
        if (interval.compare("day", Qt::CaseInsensitive) == 0 &&
            (instrument.segment == "NFO-FUT" || instrument.segment == "NFO-OPT"))
        {
            addContinuous = true;
            qDebug() << " -> Adding 'continuous=1' for daily NFO request.";
        } else {
            qDebug() << " -> Not adding 'continuous=1' (Interval:" << interval << "Segment:" << instrument.segment << ")";
        }
    } else {
        qWarning() << "DataManager instance is null, cannot determine segment for continuous flag.";
    }

    if (addContinuous) {
        query.addQueryItem("continuous", "1");
    }
    // --- End conditional logic ---

    url.setQuery(query); // Set the final query on the URL
    qDebug() << "  Final URL for historical request:" << url.toString(); // Log final URL

    // Create request with standard headers (using helper is fine here)
    QNetworkRequest request = createBaseRequest(url);
    // The createBaseRequest helper adds Authorization and X-Kite-Version
    // Its internal logging should confirm this.

    // Send the request
    m_httpManager->sendGetRequest(request, RequestType::HistoricalDataRequest);
}

// Fetches user profile details
void KiteConnectAPI::fetchUserProfile() {
    if (m_accessToken.isEmpty()) {
        qWarning() << "KiteConnectAPI::fetchUserProfile: Access token not available.";
        emit userProfileFailed("Access token not available.");
        return;
    }
    qDebug() << "KiteConnectAPI: Requesting User Profile...";
    QUrl url(m_baseUrl + "/user/profile");
    QNetworkRequest request = createBaseRequest(url);
    m_httpManager->sendGetRequest(request, RequestType::ProfileRequest);
}

// Fetches user margin details
void KiteConnectAPI::fetchUserMargins() {
    if (m_accessToken.isEmpty()) {
        qWarning() << "KiteConnectAPI::fetchUserMargins: Access token not available.";
        emit userMarginsFailed("Access token not available.");
        return;
    }
    qDebug() << "KiteConnectAPI: Requesting User Margins...";
    QUrl url(m_baseUrl + "/user/margins"); // Correct endpoint
    QNetworkRequest request = createBaseRequest(url);
    m_httpManager->sendGetRequest(request, RequestType::MarginsRequest);
}


// --- Internal Helper Methods ---

// Creates a QNetworkRequest and adds standard headers (Version, Authorization if available)
QNetworkRequest KiteConnectAPI::createBaseRequest(const QUrl& url) {
    QNetworkRequest request(url);
    qDebug() << "--- createBaseRequest for URL:" << url.toString() << "---"; // Log URL entry

    // Set Kite API version header
    request.setRawHeader("X-Kite-Version", m_apiVersion.toUtf8());
    qDebug() << "  Adding Header - X-Kite-Version:" << m_apiVersion;

    // Add Authorization header if access token exists
    if (!m_accessToken.isEmpty()) {
        QString authHeaderValue = QString("token %1:%2").arg(m_apiKey).arg(m_accessToken);
        request.setRawHeader("Authorization", authHeaderValue.toUtf8());
        // Log Authorization header carefully, masking sensitive parts
        qDebug() << "  Adding Header - Authorization: token"
                 << m_apiKey.left(4) << "...:" << m_accessToken.left(4) << "...";
    } else {
        // This is normal for the initial /session/token request, warn otherwise
        if (url.path() != "/session/token") {
            qWarning() << "  createBaseRequest: Access Token IS EMPTY when creating request!";
        } else {
            qDebug() << "  createBaseRequest: Access Token is empty (expected for session request).";
        }
    }

    // --- ADDED: Log all final headers being sent ---
    qDebug() << "  Final Request Headers:";
    QList<QByteArray> headerList = request.rawHeaderList();
    if (headerList.isEmpty()) {
        qDebug() << "    (No headers set)";
    } else {
        for(const QByteArray &headerName : headerList) {
            qDebug() << "    " << headerName << ":" << request.rawHeader(headerName);
        }
    }
    qDebug() << "--- End createBaseRequest ---";
    // --- End Added logging ---

    return request;
}


// --- Response Handling Slot ---

// Central slot connected to HttpManager::requestFinished
void KiteConnectAPI::onNetworkReply(QNetworkReply* reply, RequestType type) {
    qDebug() << "KiteConnectAPI::onNetworkReply: Called! Request Type:" << static_cast<int>(type);

    if (!reply) {
        qCritical() << "KiteConnectAPI::onNetworkReply: Received null reply object!";
        return; // Cannot proceed
    }

    // Check for Qt network errors first (e.g., connection refused, timeout)
    if (reply->error() != QNetworkReply::NoError) {
        handleNetworkReplyError(reply, type); // Handle and log the error
        reply->deleteLater();                 // Clean up the reply object
        return;
    }

    // Dispatch to the appropriate handler based on the request type
    switch (type) {
    case RequestType::SessionRequest:
        handleSessionResponse(reply);
        break;
    case RequestType::InstrumentsRequest:
        handleInstrumentsResponse(reply);
        break;
    case RequestType::HistoricalDataRequest:
    {
        // Extract context (token, interval) from the URL path
        // Assumes URL format like /instruments/historical/TOKEN/INTERVAL
        QStringList pathParts = reply->url().path().split('/');
        QString token = (pathParts.size() > 3) ? pathParts[3] : "UNKNOWN_TOKEN";
        QString interval = (pathParts.size() > 4) ? pathParts[4] : "UNKNOWN_INTERVAL";
        handleHistoricalDataResponse(reply, token, interval);
    }
    break;
    case RequestType::ProfileRequest:
        handleUserProfileResponse(reply);
        break;
    case RequestType::MarginsRequest:
        handleUserMarginsResponse(reply);
        break;
    // Add cases for other future request types (OrderRequest, etc.)
    case RequestType::InvalidRequest:
        qWarning() << "KiteConnectAPI::onNetworkReply: Received reply for InvalidRequest type.";
        // Should generally not happen if type is set correctly
        break;
    default:
        qWarning() << "KiteConnectAPI::onNetworkReply: Unhandled request type:" << static_cast<int>(type) << "for URL:" << reply->url().toString();
        // Handle unexpected types if necessary
        break;
    }

    // IMPORTANT: Delete the reply object now that processing is finished
    reply->deleteLater();
}


// --- Specific Response Handlers ---

// Handles the JSON response for session generation
void KiteConnectAPI::handleSessionResponse(QNetworkReply* reply) {
    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);

    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        qWarning() << "KiteConnectAPI::handleSessionResponse: Failed to parse JSON response:" << responseData;
        emit sessionGenerationFailed("Failed to parse session JSON response.");
        return;
    }

    QJsonObject jsonObject = jsonDoc.object();
    // Check the 'status' field in the JSON response
    if (jsonObject.value("status").toString() == "success") {
        QJsonObject data = jsonObject.value("data").toObject();
        // Extract and store access token and user ID
        m_accessToken = data.value("access_token").toString();
        m_userId = data.value("user_id").toString();
        qDebug() << "Access Token Received (First 4 chars):" << m_accessToken.left(4);
        qDebug() << "User ID:" << m_userId;

        // Persist token details using ConfigurationManager
        ConfigurationManager* config = ConfigurationManager::instance();
        if (config) {
            config->setAccessToken(m_accessToken);
            config->setAccessTokenTimestamp(QDateTime::currentDateTime());
            // config->saveConfiguration(); // Save if setters don't auto-save
        } else {
            qWarning() << "ConfigManager instance null, cannot persist access token.";
        }
        // Signal success
        emit sessionGenerated(m_accessToken);
    } else {
        // Extract error message from JSON
        QString error = jsonObject.value("message").toString("Unknown session generation error");
        qWarning() << "KiteConnectAPI::handleSessionResponse: API error -" << error;
        emit sessionGenerationFailed(error);
    }
}

// Handles the CSV response for instrument list fetching
void KiteConnectAPI::handleInstrumentsResponse(QNetworkReply* reply) {
    QByteArray responseData = reply->readAll();

    // Basic check if data seems valid (CSV is text, usually not empty on success)
    if (responseData.isEmpty()) {
        qWarning() << "KiteConnectAPI::handleInstrumentsResponse: Received empty instrument data.";
        emit instrumentsFetchFailed("Received empty instrument data.");
        return;
    }

    // Determine where to save the file (e.g., application data location)
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create application data directory:" << dirPath;
            emit instrumentsFetchFailed("Failed to create data directory.");
            return;
        }
    }
    // Create a date-stamped filename
    QString dateStr = QDateTime::currentDateTime().toString("yyyyMMdd");
    QString filePath = dir.filePath(QString("instruments_%1.csv").arg(dateStr));

    // Write the received data to the file
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { // Overwrite if exists
        file.write(responseData);
        file.close();
        qInfo() << "KiteConnectAPI: Instruments data saved successfully to:" << filePath;
        // Signal success with the path to the saved file
        emit instrumentsFetched(filePath);
    } else {
        qWarning() << "KiteConnectAPI: Failed to open file for writing instruments:" << filePath << file.errorString();
        emit instrumentsFetchFailed("Failed to save instruments file: " + file.errorString());
    }
}

// Handles the JSON response for historical data
void KiteConnectAPI::handleHistoricalDataResponse(QNetworkReply* reply, const QString& instrumentToken, const QString& interval) {
    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);

    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        qWarning() << "KiteConnectAPI::handleHistoricalDataResponse: Failed to parse JSON response for token" << instrumentToken << responseData;
        emit historicalDataFailed("Failed to parse historical JSON response", instrumentToken + "_" + interval);
        return;
    }

    QJsonObject jsonObject = jsonDoc.object();
    if (jsonObject.value("status").toString() == "success") {
        QJsonObject data = jsonObject.value("data").toObject();
        QJsonArray candles = data.value("candles").toArray(); // The actual candle data array
        qDebug() << "KiteConnectAPI: Historical data received successfully for" << instrumentToken << interval << "- Candles count:" << candles.size();
        // Emit the raw candle array for DataManager to process
        emit historicalDataReceived(instrumentToken, interval, candles);
    } else {
        QString error = jsonObject.value("message").toString("Unknown historical data error");
        qWarning() << "KiteConnectAPI::handleHistoricalDataResponse: API error for" << instrumentToken << "-" << error;
        emit historicalDataFailed(error, instrumentToken + "_" + interval);
    }
}

// Handles the JSON response for user profile fetching
void KiteConnectAPI::handleUserProfileResponse(QNetworkReply* reply) {
    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData); // Use :: scope resolution
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        qWarning() << "KiteConnectAPI::handleUserProfileResponse: Failed to parse JSON response:" << responseData;
        emit userProfileFailed("Failed to parse profile JSON response.");
        return;
    }
    QJsonObject jsonObject = jsonDoc.object();
    if (jsonObject.value("status").toString() == "success") {
        QJsonObject data = jsonObject.value("data").toObject();
        m_userId = data.value("user_id").toString(); // Store user ID locally if needed elsewhere
        qDebug() << "KiteConnectAPI: User Profile received successfully. UserID:" << m_userId;
        // Emit the profile data object
        emit userProfileReceived(data);
    } else {
        QString error = jsonObject.value("message").toString("Unknown profile error");
        qWarning() << "KiteConnectAPI::handleUserProfileResponse: API error -" << error;
        emit userProfileFailed(error);
    }
}

// Handles the JSON response for user margin fetching
void KiteConnectAPI::handleUserMarginsResponse(QNetworkReply* reply) {
    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData); // Use :: scope resolution
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        qWarning() << "KiteConnectAPI::handleUserMarginsResponse: Failed to parse JSON response:" << responseData;
        emit userMarginsFailed("Failed to parse margins JSON response.");
        return;
    }
    QJsonObject jsonObject = jsonDoc.object();
    if (jsonObject.value("status").toString() == "success") {
        QJsonObject data = jsonObject.value("data").toObject(); // Contains equity/commodity nodes
        qDebug() << "KiteConnectAPI: User Margins received successfully.";
        // Emit the margin data object
        emit userMarginsReceived(data);
    } else {
        QString error = jsonObject.value("message").toString("Unknown margins error");
        qWarning() << "KiteConnectAPI::handleUserMarginsResponse: API error -" << error;
        emit userMarginsFailed(error);
    }
}


// --- Error Handler ---

// Handles network-level errors reported by QNetworkReply
void KiteConnectAPI::handleNetworkReplyError(QNetworkReply* reply, RequestType type) {
    QString err = reply->errorString(); // Qt's description of the error
    QNetworkReply::NetworkError code = reply->error(); // The Qt network error code
    QUrl url = reply->url(); // The URL that failed
    // Get HTTP status code if available (e.g., 400, 403, 500)
    int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // Try to get a snippet of the response body for more context
    QByteArray responseBody = reply->peek(512);

    // Log detailed error information
    qCritical().noquote() << QString("Network Error: Type=%1, Code=%2, HTTP=%3, URL=%4, Error=%5, Response=%6")
                                 .arg(static_cast<int>(type))
                                 .arg(static_cast<int>(code))
                                 .arg(httpStatusCode)
                                 .arg(url.toString())
                                 .arg(err)
                                 .arg(QString::fromUtf8(responseBody));

    // Create a combined error message for signals
    QString finalDetailedError = QString("Network Error (%1): %2 (HTTP %3)")
                                     .arg(static_cast<int>(code)) // Include Qt error code
                                     .arg(err)
                                     .arg(httpStatusCode); // Include HTTP status

    // Emit the appropriate specific failure signal based on the request type
    switch (type) {
    case RequestType::SessionRequest: emit sessionGenerationFailed(finalDetailedError); break;
    case RequestType::InstrumentsRequest: emit instrumentsFetchFailed(finalDetailedError); break;
    case RequestType::HistoricalDataRequest:
    {
        QStringList pathParts = reply->url().path().split('/');
        QString token = (pathParts.size() > 3) ? pathParts[3] : "Unknown";
        QString interval = (pathParts.size() > 4) ? pathParts[4] : "Unknown";
        emit historicalDataFailed(finalDetailedError, token + "_" + interval);
    }
    break;
    case RequestType::ProfileRequest: emit userProfileFailed(finalDetailedError); break;
    case RequestType::MarginsRequest: emit userMarginsFailed(finalDetailedError); break;
    // Add cases for other request types as needed
    default:
        qWarning() << "Emitting generic API error for unhandled network error type:" << static_cast<int>(type);
        emit apiErrorOccurred(finalDetailedError); // Emit a generic signal for unhandled types
        break;
    }
}
