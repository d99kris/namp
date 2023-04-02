TARGET               = namp
TEMPLATE             = app
CONFIG              += c++11 debug cmdline
QT                  += core multimedia

HEADERS              = src/audioplayer.h                       \
                       src/common.h                            \
                       src/log.h                               \
                       src/scrobbler.h                         \
                       src/uikeyhandler.h                      \
                       src/uiview.h                            \
                       src/util.h                              \
                       src/version.h

SOURCES              = src/audioplayer.cpp                     \
                       src/main.cpp                            \
                       src/log.cpp                             \
                       src/scrobbler.cpp                       \
                       src/util.cpp

!DEVBUILD {
TARGET               = namp
INCLUDEPATH         += $$PWD/src
SOURCES             += src/uikeyhandler.cpp                    \
                       src/uiview.cpp
}

DEVBUILD {
TARGET               = nmp123
INCLUDEPATH         += $$PWD/src
SOURCES             += dev/uikeyhandler.cpp                    \
                       dev/uiview.cpp
}

LIBS                += -lncursesw                              \
                       -ltag

isEmpty(PREFIX) {
 PREFIX = /usr/local
}

target.path          = $$PREFIX/bin

manpage.path         = $$PREFIX/share/man/man1
manpage.files        = res/namp.1

INSTALLS            += target manpage

unix:!macx {
  INCLUDEPATH       += /usr/include/taglib                     \
                       /usr/include/ncursesw
}

macx: {
  INCLUDEPATH       += /usr/local/opt/ncurses/include          \
                       /usr/local/opt/taglib/include/taglib    \
                       /opt/homebrew/opt/ncurses/include       \
                       /opt/homebrew/opt/taglib/include/taglib

  LIBS              += -L/usr/local/opt/ncurses/lib            \
                       -L/usr/local/opt/taglib/lib             \
                       -L/opt/homebrew/opt/ncurses/lib         \
                       -L/opt/homebrew/opt/taglib/lib

  DEFINES           += _XOPEN_SOURCE_EXTENDED=1

  QT                += widgets

  QMAKE_INFO_PLIST   = res/InfoNoDock.plist
}
