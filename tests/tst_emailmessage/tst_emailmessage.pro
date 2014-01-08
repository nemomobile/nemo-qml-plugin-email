include(../common.pri)
TARGET = tst_emailmessage

SOURCES += tst_emailmessage.cpp

attachments.files = attachments/*
attachments.path = /opt/tests/nemo-qml-plugins/email
INSTALLS += attachments
