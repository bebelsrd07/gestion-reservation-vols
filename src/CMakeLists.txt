cmake_minimum_required(VERSION 3.16)

project(ReservationBilletsAvion VERSION 2.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Sql Network)

qt_add_executable(ReservationBilletsAvion
    main.cpp
    constantes.h
    modeles.h
    calculsvol.h
    calculsvol.cpp
    databasemanager.h
    databasemanager.cpp
    billetpdf.h
    billetpdf.cpp
    smtpclient.h
    smtpclient.cpp
    planavionwidget.h
    planavionwidget.cpp
    mainwindow.h
    mainwindow.cpp
    mainwindow.ui
)

target_link_libraries(ReservationBilletsAvion PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Sql
    Qt6::Network
)
