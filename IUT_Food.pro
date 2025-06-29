QT       += core gui sql widgets

CONFIG   += c++11

SOURCES += \
    checkoutdialog.cpp \
    main.cpp \
    client.cpp \
    profilepanel.cpp \
    databasehandler.cpp \
    restaurantitemwidget.cpp \
    shoppingcartpopup.cpp

HEADERS += \
    checkoutdialog.h \
    client.h \
    profilepanel.h \
    databasehandler.h \
    restaurantitemwidget.h \
    shoppingcartpopup.h \
    datatypes.h

FORMS += \
    checkoutdialog.ui \
    client.ui \
    restaurantitemwidget.ui
