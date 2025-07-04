QT += core gui widgets sql network charts

CONFIG   += c++11

SOURCES += \
    adminnetworkmanager.cpp \
    main.cpp \
    databasehandler.cpp \
    adminpanel.cpp \
    mainwindow.cpp \
    user.cpp \
    client.cpp \
    checkoutdialog.cpp \
    menudialog.cpp \
    menuitemwidget.cpp \
    networkmanager.cpp \
    profilepanel.cpp \
    restaurantdelegate.cpp \
    restaurantitemwidget.cpp \
    restaurantlistdialog.cpp \
    restaurantmodel.cpp \
    shoppingcart.cpp \
    shoppingcartpopup.cpp \
    restaurant.cpp \
    restaurantwindow.cpp

HEADERS += \
    adminnetworkmanager.h \
    databasehandler.h \
    adminpanel.h \
    mainwindow.h \
    user.h \
    client.h \
    checkoutdialog.h \
    datatypes.h \
    menudialog.h \
    menuitemwidget.h \
    networkmanager.h \
    profilepanel.h \
    restaurantdelegate.h \
    restaurantitemwidget.h \
    restaurantlistdialog.h \
    restaurantmodel.h \
    shoppingcart.h \
    shoppingcartpopup.h \
    fooditem.h \
    order.h \
    restaurant.h \
    restaurantwindow.h

FORMS += \
    mainwindow.ui \
    client.ui \
    checkoutdialog.ui \
    menudialog.ui \
    menuitemwidget.ui \
    restaurantitemwidget.ui \
    restaurantlistdialog.ui

QMAKE_CXXFLAGS += -fexec-charset=UTF-8
QMAKE_CXXFLAGS += -finput-charset=UTF-8
