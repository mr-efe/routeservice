QT += core network
QT -= gui

CONFIG += c++17

DESTDIR = $$OUT_PWD/../../output

TARGET = routeservice

CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
	discoveryservice.cpp \
	macdservice.cpp \
	routemanager.cpp \
	udpsocket.cpp

HEADERS += \
	discoveryservice.h \
	macdservice.h \
	routemanager.h \
	udpsocket.h

LIBS += -liphlpapi -ladvapi32 -lws2_32

include($$PWD/../logging/logging.pri)
