#ifndef KITE_CONNECT_API_H
#define KITE_CONNECT_API_H

#include <QObject>
#include <QString>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QQueue>
#include <QMetaType> // Include for Q_DECLARE_METATYPE

// Forward declaration
class HttpManager;
class ConfigurationManager;

/**
 * @brief Defines the types of API requests managed by KiteConnectAPI.
 */
enum class RequestType {
    InvalidRequest = 0,         ///< Default or error type
    InstrumentsRequest = 1,     ///< Fetching instrument list (CSV)
    HistoricalDataRequest = 2,  ///< Fetching historical candles (JSON)
    SessionRequest = 3,         ///< Generating session/access token (JSON)
    ProfileRequest = 4,         ///< Fetching user profile (JSON)
    MarginsRequest = 5,         ///< Fetching user margins (JSON)
    OrderRequest = 6,           ///< Placeholder for Order related requests
    QuoteRequest = 7,           ///< Placeholder for Quote requests
    HoldingsRequest = 8,        ///< Placeholder for Holdings requests
    PositionsRequest = 9        ///< Placeholder for Positions requests
};
// Make RequestType usable with QVariant for storing in QObject properties
Q_DECLARE_METATYPE(RequestType)


/**
 * @brief Facade class for interacting with the Zerodha Kite Connect API.
 *
 * Handles authentication, session management, data fetching (instruments, historical, profile, margins),
 * and potentially order management in the future. Uses HttpManager for network communication.
 */
class KiteConnectAPI : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor. Requires the API key. Fetches the API secret internally.
     * @param apiKey User's Kite Connect API key.
     * @param parent Optional parent QObject.
     */
    explicit KiteConnectAPI(const QString& apiKey, QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~KiteConnectAPI();

    // --- Public API Methods ---

    /**
     * @brief Initiates the login flow by emitting requiresUserLoginRedirect.
     */
    void login();

    /**
     * @brief Exchanges the request token for an access token and user ID.
     * @param requestToken The request token obtained after successful web login.
     */
    void generateSession(const QString& requestToken);

    /**
     * @brief Fetches the complete list of instruments (CSV).
     */
    void fetchAllInstruments();

    /**
     * @brief Fetches historical candle data for a given instrument and interval.
     * @param instrumentToken The token of the instrument.
     * @param interval The candle interval (e.g., "5minute", "day").
     * @param from The start datetime string (yyyy-MM-dd+HH:mm:ss).
     * @param to The end datetime string (yyyy-MM-dd+HH:mm:ss).
     */
    void fetchHistoricalData(const QString& instrumentToken, const QString& interval, const QString& from, const QString& to);

    /**
     * @brief Fetches the user's profile information.
     */
    void fetchUserProfile();

    /**
     * @brief Fetches the user's margin information.
     */
    void fetchUserMargins();

    // --- Accessors ---

    /**
     * @brief Returns the currently stored access token.
     * @return Access token string, or empty if not authenticated.
     */
    QString getAccessToken() const;

    /**
     * @brief Returns the API key used by this instance.
     * @return API key string.
     */
    QString getApiKey() const;


signals:
    // --- API Result Signals ---

    /**
     * @brief Emitted after successfully generating an access token.
     * @param accessToken The newly generated access token.
     */
    void sessionGenerated(const QString& accessToken);

    /**
     * @brief Emitted if access token generation fails.
     * @param error Error message description.
     */
    void sessionGenerationFailed(const QString& error);

    /**
     * @brief Emitted after successfully downloading the instruments CSV file.
     * @param filePath The path to the saved CSV file.
     */
    void instrumentsFetched(const QString& filePath);

    /**
     * @brief Emitted if fetching or saving the instruments file fails.
     * @param error Error message description.
     */
    void instrumentsFetchFailed(const QString& error);

    /**
     * @brief Emitted after successfully receiving historical candle data.
     * @param instrumentToken The token for which data was received.
     * @param interval The interval for which data was received.
     * @param candles The array of candle data (JSON format from Kite).
     */
    void historicalDataReceived(const QString& instrumentToken, const QString& interval, const QJsonArray& candles);

    /**
     * @brief Emitted if fetching historical data fails.
     * @param error Error message description.
     * @param context String indicating the token and interval for context (e.g., "TOKEN_INTERVAL").
     */
    void historicalDataFailed(const QString& error, const QString& context);

    /**
     * @brief Emitted after successfully fetching user profile data.
     * @param profileData QJsonObject containing the user's profile details.
     */
    void userProfileReceived(const QJsonObject& profileData);

    /**
     * @brief Emitted after successfully fetching user margin data.
     * @param marginData QJsonObject containing margin details (equity, commodity).
     */
    void userMarginsReceived(const QJsonObject& marginData);

    /**
     * @brief Emitted if fetching the user profile fails.
     * @param error Error message description.
     */
    void userProfileFailed(const QString& error);

    /**
     * @brief Emitted if fetching user margins fails.
     * @param error Error message description.
     */
    void userMarginsFailed(const QString& error);

    /**
     * @brief Emitted for other generic API errors not covered by specific signals.
     * @param error Error message description.
     */
    void apiErrorOccurred(const QString& error);

    /**
     * @brief Emitted when the user needs to be redirected to the Kite login page.
     * @param url The Kite login URL with API key parameters.
     */
    void requiresUserLoginRedirect(const QUrl& url);


private slots:
    /**
     * @brief Central slot connected to HttpManager::requestFinished.
     * Handles dispatching the network reply to the appropriate handler based on RequestType.
     * @param reply Pointer to the completed QNetworkReply.
     * @param type The RequestType associated with the reply.
     */
    void onNetworkReply(QNetworkReply* reply, RequestType type);

private:
    // --- Internal Helper Methods ---

    /**
     * @brief Creates a QNetworkRequest with standard headers (Auth, Version).
     * @param url The target URL for the request.
     * @return Configured QNetworkRequest object.
     */
    QNetworkRequest createBaseRequest(const QUrl& url);

    /** @brief Handles the response for a SessionRequest. */
    void handleSessionResponse(QNetworkReply* reply);
    /** @brief Handles the response for an InstrumentsRequest. */
    void handleInstrumentsResponse(QNetworkReply* reply);
    /** @brief Handles the response for a HistoricalDataRequest. */
    void handleHistoricalDataResponse(QNetworkReply* reply, const QString& instrumentToken, const QString& interval);
    /** @brief Handles the response for a ProfileRequest. */
    void handleUserProfileResponse(QNetworkReply* reply);
    /** @brief Handles the response for a MarginsRequest. */
    void handleUserMarginsResponse(QNetworkReply* reply);
    /** @brief Handles network errors reported by QNetworkReply. */
    void handleNetworkReplyError(QNetworkReply* reply, RequestType type);

    // --- Member Variables ---
    HttpManager* m_httpManager;     // Handles actual HTTP communication.
    QString m_apiKey;               // User's API key.
    QString m_apiSecret;            // User's API secret (fetched from config).
    QString m_accessToken;          // Session access token.
    QString m_userId;               // User's ID (from session/profile).

    // --- Constants ---
    const QString m_baseUrl = "https://api.kite.trade";             ///< Base URL for API calls.
    const QString m_loginUrl = "https://kite.zerodha.com/connect/login"; ///< URL for web login initiation.
    const QString m_apiVersion = "3";                               ///< Kite Connect API version.
};

#endif // KITE_CONNECT_API_H
