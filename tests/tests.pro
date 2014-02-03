TEMPLATE = subdirs
SUBDIRS = \
    tst_emailfolder \
    tst_emailmessage
    

tests_xml.target = tests.xml
tests_xml.files = tests.xml
tests_xml.path = /opt/tests/nemo-qml-plugins/email
INSTALLS += tests_xml
