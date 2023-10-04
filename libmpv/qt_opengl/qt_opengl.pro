CONFIG -= app_bundle
QT += openglwidgets

QT_CONFIG -= no-pkg-config
CONFIG += link_pkgconfig debug
PKGCONFIG += mpv

HEADERS = \
    mpvwidget.h \
    mainwindow.h
SOURCES = main.cpp \
    mpvwidget.cpp \
    mainwindow.cpp
