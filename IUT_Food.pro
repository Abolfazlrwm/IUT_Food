QT       += core gui sql widgets network

CONFIG   += c++11

SOURCES += \
    checkoutdialog.cpp \
    main.cpp \
    client.cpp \
    menudialog.cpp \
    menuitemwidget.cpp \
    networkmanager.cpp \
    profilepanel.cpp \
    databasehandler.cpp \
    restaurantitemwidget.cpp \
    shoppingcart.cpp \
    shoppingcartpopup.cpp

HEADERS += \
    checkoutdialog.h \
    client.h \
    menudialog.h \
    menuitemwidget.h \
    networkmanager.h \
    profilepanel.h \
    databasehandler.h \
    restaurantitemwidget.h \
    shoppingcart.h \
    shoppingcartpopup.h \
    datatypes.h

FORMS += \
    checkoutdialog.ui \
    client.ui \
    menudialog.ui \
    menuitemwidget.ui \
    restaurantitemwidget.ui
    # Force the compiler to handle source code and execution as UTF-8
    # This is often necessary for MinGW on Windows to display non-English characters correctly.
    QMAKE_CXXFLAGS += -fexec-charset=UTF-8
    QMAKE_CXXFLAGS += -finput-charset=UTF-8
