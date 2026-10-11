QT += core gui widgets network sql charts

CONFIG   += c++17

TARGET   = iut-food-client

INCLUDEPATH += ../common ../../resources

SOURCES += \
    main.cpp \
    adminpanel.cpp \
    client.cpp \
    checkoutdialog.cpp \
    logindialog.cpp \
    mainwindow.cpp \
    menudialog.cpp \
    menuitemwidget.cpp \
    networkmanager.cpp \
    ordertrackerdialog.cpp \
    profilepanel.cpp \
    restaurant.cpp \
    restaurantdelegate.cpp \
    restaurantitemwidget.cpp \
    restaurantlistdialog.cpp \
    restaurantmodel.cpp \
    restaurantwindow.cpp \
    shoppingcart.cpp \
    shoppingcartpopup.cpp \
    thememanager.cpp \
    ../common/databasehandler.cpp \
    ../common/demodata.cpp \
    ../common/user.cpp

HEADERS += \
    adminpanel.h \
    client.h \
    checkoutdialog.h \
    logindialog.h \
    mainwindow.h \
    menudialog.h \
    menuitemwidget.h \
    networkmanager.h \
    ordertrackerdialog.h \
    profilepanel.h \
    restaurant.h \
    restaurantdelegate.h \
    restaurantitemwidget.h \
    restaurantlistdialog.h \
    restaurantmodel.h \
    restaurantwindow.h \
    shoppingcart.h \
    shoppingcartpopup.h \
    thememanager.h \
    ../common/databasehandler.h \
    ../common/demodata.h \
    ../common/datatypes.h \
    ../common/fooditem.h \
    ../common/order.h \
    ../common/orderstatus.h \
    ../common/user.h

FORMS += \
    client.ui \
    checkoutdialog.ui \
    mainwindow.ui \
    menudialog.ui \
    menuitemwidget.ui \
    restaurantitemwidget.ui \
    restaurantlistdialog.ui

# Icons (WebP) and the bundled Vazirmatn typeface.
RESOURCES += ../../resources/resources.qrc
