TEMPLATE = subdirs

src.subdir = src
src.target = sub-src

plugins.subdir = src/plugin
plugins.target = sub-plugin
plugins.depends = sub-src

tests.subdir = tests
tests.taget = sub-tests
tests.depends = sub-src

SUBDIRS = src plugins tests

OTHER_FILES += rpm/nemo-qml-plugin-email-qt5.spec \
              configurations/domainSettings.conf \
              configurations/serviceSettings.conf

conf.files += configurations/domainSettings.conf \
              configurations/serviceSettings.conf
conf.path += /etc/xdg/nemo-qml-plugin-email
INSTALLS += conf
