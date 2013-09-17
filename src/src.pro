TEMPLATE = lib
QT += network
QT -= gui
CONFIG += link_pkgconfig

equals(QT_MAJOR_VERSION, 4) {
    TARGET = nemoemail
    PKGCONFIG += qmfmessageserver qmfclient
}

equals(QT_MAJOR_VERSION, 5) {
    TARGET = nemoemail-qt5
    PKGCONFIG += qmfmessageserver5 qmfclient5
}

CONFIG += qt hide_symbols create_pc create_prl

SOURCES += \
    $$PWD/emailaccountlistmodel.cpp \
    $$PWD/emailmessagelistmodel.cpp \
    $$PWD/folderlistmodel.cpp \
    $$PWD/emailagent.cpp \
    $$PWD/emailmessage.cpp \
    $$PWD/emailaccountsettingsmodel.cpp \
    $$PWD/emailaccount.cpp \
    $$PWD/emailaction.cpp \
    $$PWD/emailfolder.cpp \
    $$PWD/attachmentlistmodel.cpp

HEADERS += \
    $$PWD/emailaccountlistmodel.h \
    $$PWD/emailmessagelistmodel.h \
    $$PWD/folderlistmodel.h \
    $$PWD/emailagent.h \
    $$PWD/emailmessage.h \
    $$PWD/emailaccountsettingsmodel.h \
    $$PWD/emailaccount.h \
    $$PWD/emailaction.h \
    $$PWD/emailfolder.h \
    $$PWD/attachmentlistmodel.h

MOC_DIR = $$PWD/../.moc
OBJECTS_DIR = $$PWD/../.obj

target.path = $$[QT_INSTALL_LIBS]
pkgconfig.files = $$TARGET.pc
pkgconfig.path = $$target.path/pkgconfig
headers.files = $$HEADERS
equals(QT_MAJOR_VERSION, 4): headers.path = /usr/include/nemoemail
equals(QT_MAJOR_VERSION, 5): headers.path = /usr/include/nemoemail-qt5

VERSION = 0.0.1
QMAKE_PKGCONFIG_NAME = lib$$TARGET
QMAKE_PKGCONFIG_DESCRIPTION = Email plugin for Nemo Mobile
QMAKE_PKGCONFIG_LIBDIR = $$target.path
QMAKE_PKGCONFIG_INCDIR = $$headers.path
QMAKE_PKGCONFIG_DESTDIR = pkgconfig

INSTALLS += target headers pkgconfig
