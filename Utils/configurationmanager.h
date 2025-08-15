#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

class ConfigurationManager : public QObject
{
    Q_OBJECT

public:
    static ConfigurationManager* instance();

    void loadConfiguration(const QString &configFile);
    void saveConfiguration();

    QString getApiKey() const;
    QString getApiSecret() const;
    QJsonObject getStrategyConfig(const QString &strategyName) const;
    QJsonObject getRiskParameters() const;
    QJsonArray getHolidays() const;
    void setHolidays(const QJsonArray &holidays);
    QString getAccessToken() const;
    void setAccessToken(const QString &token);
    QDateTime getAccessTokenTimestamp() const;
    void setAccessTokenTimestamp(const QDateTime &timestamp);

    // Add these:
    void setApiKey(const QString &apiKey);
    void setApiSecret(const QString &apiSecret);

private:
    explicit ConfigurationManager(QObject *parent = nullptr);
    ~ConfigurationManager();

    static ConfigurationManager* m_instance;
    QJsonObject m_configData; // Add this line
    QString m_configFilePath;
};

#endif // CONFIGURATIONMANAGER_H
