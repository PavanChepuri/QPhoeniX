#include "logindialog.h"
#include <QUrlQuery>
#include <QDebug>
#include <QVBoxLayout>
#include <QWebEnginePage>

// Constructor for LoginDialog.
LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent)
{
    // Set up the dialog layout
    QVBoxLayout *layout = new QVBoxLayout(this); // Correctly use QVBoxLayout
    m_webView = new QWebEngineView(this);
    layout->addWidget(m_webView);
    setLayout(layout);  // *** ESSENTIAL: Set the main layout for the dialog ***
    m_webView->setPage(new QWebEnginePage(this)); //Set QWebEnginePage for the QWebEngineView

    // Set the size of the dialog
    this->setMinimumSize(1000, 800); // Larger minimum size

    // Connect the URL changed signal, using a lambda for simplicity and correctness.
    connect(m_webView->page(), &QWebEnginePage::urlChanged, this, &LoginDialog::handleUrlChanged);

}

// Destructor for LoginDialog.
LoginDialog::~LoginDialog()
{
    // No need to delete m_webView, it's a child of the dialog and will be deleted automatically
}

// Loads the specified URL into the web view.
void LoginDialog::loadLoginPage(const QUrl &url)
{
    m_webView->load(url);

    // Connect to loadFinished() signal to resize after loading
    connect(m_webView, &QWebEngineView::loadFinished, this, &LoginDialog::adjustDialogSize);
}

// Slot to handle changes in the URL of the web view.
void LoginDialog::handleUrlChanged(const QUrl &url)
{
    qDebug() << "URL Changed:" << url.toString();
    QUrlQuery query(url);
    if (query.hasQueryItem("request_token")) {
        QString requestToken = query.queryItemValue("request_token");
        qDebug() << "Request Token:" << requestToken;
        emit requestTokenReceived(requestToken);
        this->hide(); //hide after emitting.
    } else if (url.host() == "kite.trade" && url.path()=="/connect/login"){
        // do nothing as kite.trade loading is normal
    }
    else if (query.hasQueryItem("action") && query.queryItemValue("action") == "login" &&
             query.hasQueryItem("status") && query.queryItemValue("status") == "error")
    {
        qDebug() << "Login Failed due to : " << url;
        emit loginFailed("Login Failed"); //Added
        this->hide();
    }
}

void LoginDialog::adjustDialogSize()
{
    this->adjustSize();

    //Optionally, set a maximum size to prevent the dialog from becoming too large
    this->setMaximumSize(QSize(1200, 900)); // Example maximum size
}
