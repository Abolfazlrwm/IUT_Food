cmake_minimum_required(VERSION 3.16)

project(IUT-Food
    VERSION 2.0.0
    DESCRIPTION "Campus food-ordering system built with Qt"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt 6 first, Qt 5.15 as a fallback.
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Network Sql Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network Sql Charts)

# ----------------------------------------------------------------------
# Shared data layer used by both executables
# ----------------------------------------------------------------------
add_library(iutfood_core STATIC
    src/common/databasehandler.cpp
    src/common/demodata.cpp
    src/common/user.cpp
    src/common/databasehandler.h
    src/common/demodata.h
    src/common/datatypes.h
    src/common/fooditem.h
    src/common/order.h
    src/common/orderstatus.h
    src/common/user.h
)
target_include_directories(iutfood_core PUBLIC src/common)
target_link_libraries(iutfood_core
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql)

# ----------------------------------------------------------------------
# Client (GUI application)
# ----------------------------------------------------------------------
add_executable(iut-food-client WIN32 MACOSX_BUNDLE
    src/client/main.cpp
    src/client/mainwindow.cpp    src/client/mainwindow.h    src/client/mainwindow.ui
    src/client/logindialog.cpp   src/client/logindialog.h
    src/client/client.cpp        src/client/client.h        src/client/client.ui
    src/client/profilepanel.cpp  src/client/profilepanel.h
    src/client/restaurant.cpp    src/client/restaurant.h
    src/client/restaurantwindow.cpp src/client/restaurantwindow.h
    src/client/adminpanel.cpp    src/client/adminpanel.h
    src/client/networkmanager.cpp src/client/networkmanager.h
    src/client/shoppingcart.cpp  src/client/shoppingcart.h
    src/client/shoppingcartpopup.cpp src/client/shoppingcartpopup.h
    src/client/checkoutdialog.cpp src/client/checkoutdialog.h src/client/checkoutdialog.ui
    src/client/menudialog.cpp    src/client/menudialog.h    src/client/menudialog.ui
    src/client/menuitemwidget.cpp src/client/menuitemwidget.h src/client/menuitemwidget.ui
    src/client/restaurantlistdialog.cpp src/client/restaurantlistdialog.h src/client/restaurantlistdialog.ui
    src/client/restaurantmodel.cpp src/client/restaurantmodel.h
    src/client/restaurantdelegate.cpp src/client/restaurantdelegate.h
    src/client/restaurantitemwidget.cpp src/client/restaurantitemwidget.h src/client/restaurantitemwidget.ui
    # AUTORCC compiles the qrc (icons + bundled fonts) into the binary:
    resources/resources.qrc
)
target_link_libraries(iut-food-client PRIVATE
    iutfood_core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Charts)

# ----------------------------------------------------------------------
# Server (console application)
# ----------------------------------------------------------------------
add_executable(iut-food-server
    src/server/main.cpp
    src/server/server.cpp        src/server/server.h
    src/server/connectionhandler.cpp src/server/connectionhandler.h
)
target_link_libraries(iut-food-server PRIVATE
    iutfood_core
    Qt${QT_VERSION_MAJOR}::Network)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/*.h)

install(TARGETS iut-food-client iut-food-server
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION bin)
