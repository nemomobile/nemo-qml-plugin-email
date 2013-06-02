TARGET = nemoemail
PLUGIN_IMPORT_PATH = org/nemomobile/email

TEMPLATE = lib
CONFIG += qt plugin hide_symbols
QT += declarative

target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$[QT_INSTALL_IMPORTS]/$$$$PLUGIN_IMPORT_PATH
INSTALLS += qmldir

QT += network
CONFIG += link_pkgconfig

PKGCONFIG += qmfmessageserver \
    qmfclient

packagesExist(mlite) {
    PKGCONFIG += mlite
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

