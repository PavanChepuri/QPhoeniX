QT += core gui charts webenginewidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

DEFINES += DEVELOPMENT

SOURCES += \
    Data/accountdata.cpp \
    Data/datamanager.cpp \
    Data/marketdatacache.cpp \
    Network/httpmanager.cpp \
    Network/kiteconnectapi.cpp \
    Network/kitewebsocket.cpp \
    OrderManagement/ordermanager.cpp \
    RiskManagement/riskmanager.cpp \
    Strategies/daytradingstrategy1.cpp \
    Strategies/expirydaystrategy.cpp \
    Strategies/positionalstrategy1.cpp \
    Strategies/strategyfactory.cpp \
    Strategies/strategymanager.cpp \
    UI/accountinfowidget.cpp \
    UI/chartview.cpp \
    UI/logindialog.cpp \
    UI/mainwindow.cpp \
    UI/orderlogwidget.cpp \
    UI/strategyconfigwidget.cpp \
    UI/watchlistwidget.cpp \
    Utils/configurationmanager.cpp \
    Utils/logger.cpp \
    Utils/marketcalendar.cpp \
    Utils/ta_simple.cpp \
    main.cpp

HEADERS += \
    Data/DataStructures/instrumentanalytics.h \
    Data/accountdata.h \
    Data/datamanager.h \
    Data/marketdatacache.h \
    Data/DataStructures/candle.h \
    Data/DataStructures/historicaldata.h \
    Data/DataStructures/holding.h \
    Data/DataStructures/instrumentdata.h \
    Data/DataStructures/margins.h \
    Data/DataStructures/position.h \
    Data/DataStructures/quotedata.h \
    Network/httpmanager.h \
    Network/kiteconnectapi.h \
    Network/kitewebsocket.h \
    OrderManagement/ordermanager.h \
    RiskManagement/riskmanager.h \
    Strategies/daytradingstrategy1.h \
    Strategies/expirydaystrategy.h \
    Strategies/positionalstrategy1.h \
    Strategies/strategyfactory.h \
    Strategies/strategyinterface.h \
    Strategies/strategymanager.h \
    UI/accountinfowidget.h \
    UI/chartview.h \
    UI/logindialog.h \
    UI/mainwindow.h \
    UI/orderlogwidget.h \
    UI/strategyconfigwidget.h \
    UI/watchlistwidget.h \
    Utils/configurationmanager.h \
    Utils/logger.h \
    Utils/marketcalendar.h \
    Utils/ta_simple.h

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    Config/config.json \
    Logs/QphoeniX.log

FORMS += \
    UI/mainwindow.ui
