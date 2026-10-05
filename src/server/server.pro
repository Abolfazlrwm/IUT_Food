QT += core network sql

CONFIG   += c++17

TARGET   = iut-food-server

INCLUDEPATH += ../common

SOURCES += \
    main.cpp \
    connectionhandler.cpp \
    server.cpp \
    ../common/databasehandler.cpp \
    ../common/demodata.cpp \
    ../common/user.cpp

HEADERS += \
    connectionhandler.h \
    server.h \
    ../common/databasehandler.h \
    ../common/demodata.h \
    ../common/datatypes.h \
    ../common/fooditem.h \
    ../common/logger.h \
    ../common/order.h \
    ../common/orderstatus.h \
    ../common/passwordhasher.h \
    ../common/ratelimiter.h \
    ../common/sessionmanager.h \
    ../common/user.h
