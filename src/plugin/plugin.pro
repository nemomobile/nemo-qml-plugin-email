TEMPLATE = lib
TARGET = nemoemail
PLUGIN_IMPORT_PATH = org/nemomobile/email
QT -= gui
CONFIG += qt plugin hide_symbols link_pkgconfig

INCLUDEPATH += ..
LIBS += -L..

equals(QT_MAJOR_VERSION, 4) {
    QT += declarative
    PKGCONFIG += qmfmessageserver qmfclient
    LIBS += -lnemoemail
    target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
}

equals(QT_MAJOR_VERSION, 5) {
    QT += qml
    PKGCONFIG += qmfmessageserver5 qmfclient5
    LIBS += -lnemoemail-qt5
    target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
}

SOURCES += plugin.cpp
OTHER_FILES += qmldir

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += target qmldir

