# Top-level qmake project: open this file in Qt Creator to build both apps.
TEMPLATE = subdirs
CONFIG   += ordered

SUBDIRS += \
    src/client/client.pro \
    src/server/server.pro
