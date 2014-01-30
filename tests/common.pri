include(../src/src.pro)
SRCDIR = ../../src/
INCLUDEPATH += $$SRCDIR
DEPENDPATH = $$INCLUDEPATH
QT += testlib
TEMPLATE = app
CONFIG -= app_bundle

CONFIG += testcase link_pkgconfig
PKGCONFIG += qmfmessageserver5 qmfclient5

target.path = /opt/tests/nemo-qml-plugins/email
INSTALLS += target
