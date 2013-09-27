TEMPLATE = subdirs

src.subdir = src
src.target = sub-src

plugins.subdir = src/plugin
plugins.target = sub-plugin
plugins.depends = sub-src

SUBDIRS = src plugins

OTHER_FILES += rpm/*
