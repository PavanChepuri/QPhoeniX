#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QQueue>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QUrl>
#include <QQueue>
#include <QString>

// Forward Declarations
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class LoginDialog;
class KiteConnectAPI;
class DataManager;
class QChartView;
class QLineSeries;

#include "Data/DataStructures/InstrumentData.h"

struct HistoricalRequestInfo {
    QString instrumentToken;
    QString interval;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setKiteConnectAPI(KiteConnectAPI *api);

private slots:
    // UI Slots / Login Flow / Profile+Margins / Data Flow Slots ...
    // (Declarations remain the same as previous correct version)
    void onLoginClicked();
    void onInstrumentSelected(int index);
    void onIntervalSelected(int index);
    void updateInstrumentsTable();
    void updateChart();
    void onRedirectUserForLogin(const QUrl& url);
    void onLoginSuccessful(const QString &accessToken);
    void onLoginFailed(const QString &error);
    void handleLoginDialogSuccess(const QString& requestToken);
    void handleLoginDialogFailure(const QString& error);
    void handleLoginDialogFinished(int result);
    void requestUserProfile();
    void onUserProfileReceived(const QJsonObject& profileData);
    void requestUserMargins();
    void onUserMarginsReceived(const QJsonObject& marginData);
    void requestInstruments();
    void onProfileOrMarginsFailed(const QString& context, const QString& error);
    void onInstrumentsFetched(const QString& filePath);
    void onInstrumentsFetchFailed(const QString& error);
    void onDataManagerReady();
    void onHistoricalDataReceived(const QString& instrumentToken, const QString& interval, const QJsonArray& candles);
    void onHistoricalDataFailed(const QString& error, const QString& context);

    // Historical Data Queue Processing Slot (Now called sequentially)
    void processNextHistoricalDataRequest();

private:
    // Helper methods ... (remain the same)
    void setupConnections();
    // *** MODIFIED *** Renamed for clarity, now uses status bar
    void showStatusMessage(const QString& message, int timeout = 0); // timeout 0 means persistent
    void populateInstrumentCombo();
    void populateIntervalCombo();
    void enqueueHistoricalDataRequests();
    void startHistoricalDataProcessing(); // Will now just kick off the first request
    void resetUserInfo(); // Helper to clear user/funds info

    // Member variables ...
    Ui::MainWindow *ui;
    LoginDialog *m_loginDialog;
    KiteConnectAPI *m_kiteApi;
    DataManager *m_dataManager;

    // QTimer *m_historicalDataTimer; // *** REMOVED *** No longer needed as member
    QQueue<HistoricalRequestInfo> m_historicalDataRequests; // Queue remains
    QMap<QString, InstrumentData> m_localInstrumentMap;

    // *** ADDED *** Members to store user/account info
    QString m_userName;
    QString m_userId;
    double m_availableFunds;
    bool m_profileReceived = false; // Flags to track received data
    bool m_marginsReceived = false;

    // Chart related members ...
};
#endif // MAINWINDOW_H
