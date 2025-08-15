#include "configurationmanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

ConfigurationManager* ConfigurationManager::m_instance = nullptr;

ConfigurationManager* ConfigurationManager::instance()
{
    if (!m_instance) {
        m_instance = new ConfigurationManager();
    }
    return m_instance;
}

ConfigurationManager::ConfigurationManager(QObject *parent) : QObject(parent)
{
}

ConfigurationManager::~ConfigurationManager()
{
}
void ConfigurationManager::loadConfiguration(const QString &configFile)
{
    m_configFilePath = configFile;
    QFile file(configFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open config file: " << configFile;
        // Create a default configuration
        m_configData = QJsonObject{
            {"api_key", ""},
            {"api_secret", ""},
            {"access_token", ""},
            {"access_token_timestamp", ""},
            {"strategies", QJsonObject()},
            {"risk_parameters", QJsonObject()},
            {"holidays", QJsonArray()}
        };
        saveConfiguration(); // Save the default config
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (!jsonDoc.isObject()) {
        qDebug() << "Invalid JSON format in config file.";
        return;
    }

    m_configData = jsonDoc.object(); // Store the entire config object
}


void ConfigurationManager::saveConfiguration()
{
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open config file for writing: " << m_configFilePath;
        return;
    }

    QJsonDocument jsonDoc(m_configData);
    file.write(jsonDoc.toJson());
    file.close();
}

QString ConfigurationManager::getApiKey() const
{
    return m_configData["api_key"].toString();
}

QString ConfigurationManager::getApiSecret() const
{
    return m_configData["api_secret"].toString();
}

QJsonObject ConfigurationManager::getStrategyConfig(const QString &strategyName) const
{
    return m_configData["strategies"].toObject()[strategyName].toObject();
}

QJsonObject ConfigurationManager::getRiskParameters() const
{
    return m_configData["risk_parameters"].toObject();
}

QJsonArray ConfigurationManager::getHolidays() const
{
    return m_configData["holidays"].toArray();
}

void ConfigurationManager::setHolidays(const QJsonArray &holidays)
{
    m_configData["holidays"] = holidays;
    saveConfiguration(); // Save changes to the configuration file.
}

QString ConfigurationManager::getAccessToken() const
{
    return m_configData["access_token"].toString();
}

void ConfigurationManager::setAccessToken(const QString &token)
{
    m_configData["access_token"] = token;
    saveConfiguration();
}

QDateTime ConfigurationManager::getAccessTokenTimestamp() const
{
    return QDateTime::fromString(m_configData["access_token_timestamp"].toString(), Qt::ISODate);
}

void ConfigurationManager::setAccessTokenTimestamp(const QDateTime &timestamp)
{
    m_configData["access_token_timestamp"] = timestamp.toString(Qt::ISODate);
    saveConfiguration();
}

void ConfigurationManager::setApiKey(const QString &apiKey)
{
    m_configData["api_key"] = apiKey;
    saveConfiguration();
}
void ConfigurationManager::setApiSecret(const QString &apiSecret)
{
    m_configData["api_secret"] = apiSecret;
    saveConfiguration();
}
