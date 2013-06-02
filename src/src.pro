TARGET = nemoemail
PLUGIN_IMPORT_PATH = org/nemomobile/email

TEMPLATE = lib
CONFIG += qt plugin hide_symbols
equals(QT_MAJOR_VERSION, 4): QT += declarative
equals(QT_MAJOR_VERSION, 5): QT += qml

equals(QT_MAJOR_VERSION, 4): target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
equals(QT_MAJOR_VERSION, 5): target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

QT += network
QT -= gui
CONFIG += link_pkgconfig

equals(QT_MAJOR_VERSION, 4): PKGCONFIG += qmfmessageserver qmfclient
equals(QT_MAJOR_VERSION, 5): PKGCONFIG += qmfmessageserver5 qmfclient5

packagesExist(mlite) | packagesExist(mlite-qt5) {
    equals(QT_MAJOR_VERSION, 4): PKGCONFIG += mlite
    equals(QT_MAJOR_VERSION, 5): PKGCONFIG += mlite5
    DEFINES += HAS_MLITE
} else {
    warning("mlite not available. Some functionality may not work as expected.")
}

SOURCES += \
    $$PWD/emailaccountlistmodel.cpp \
    $$PWD/emailmessagelistmodel.cpp \
    $$PWD/folderlistmodel.cpp \
    $$PWD/emailagent.cpp \
    $$PWD/emailmessage.cpp \
    $$PWD/emailaccountsettingsmodel.cpp \
    $$PWD/emailaccount.cpp \
    $$PWD/plugin.cpp \
    emailaction.cpp

HEADERS += \
    $$PWD/emailaccountlistmodel.h \
    $$PWD/emailmessagelistmodel.h \
    $$PWD/folderlistmodel.h \
    $$PWD/emailagent.h \
    $$PWD/emailmessage.h \
    $$PWD/emailaccountsettingsmodel.h \
    $$PWD/emailaccount.h \
    $$PWD/emailaction.h

MOC_DIR = $$PWD/../.moc
OBJECTS_DIR = $$PWD/../.obj

