#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QUrl>
#include <QVBoxLayout> // Include QVBoxLayout
#include <QWebEngineView>
#include <QWebEnginePage> // Include QWebEnginePage


// The LoginDialog class handles the web-based login process using a QWebEngineView.
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    // Constructor for LoginDialog.
    explicit LoginDialog(QWidget *parent = nullptr);

    // Destructor for LoginDialog.
    ~LoginDialog();

    // Loads the specified URL into the web view.
    void loadLoginPage(const QUrl &url);

signals:

    // Signal emitted when the request token is received.
    void requestTokenReceived(const QString &requestToken);
    void loginFailed(const QString &error);

private slots:
    // Slot to handle changes in the URL of the web view.
    void handleUrlChanged(const QUrl &url);
    void adjustDialogSize();

private:
    QVBoxLayout *m_layout;
    // The web view for displaying the login page.
    QWebEngineView *m_webView;
};

#endif // LOGINDIALOG_H
