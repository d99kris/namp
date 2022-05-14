TARGET               = namp
TEMPLATE             = app
CONFIG              += c++11 debug
QT                  += core multimedia

DEFINES             += VERSION="\\\"2.21\\\""

HEADERS              = src/audioplayer.h                       \
                       src/common.h                            \
                       src/log.h                               \
                       src/scrobbler.h                         \
                       src/uikeyhandler.h                      \
                       src/uiview.h                            \
                       src/util.h

SOURCES              = src/audioplayer.cpp                     \
                       src/main.cpp                            \
                       src/log.cpp                             \
                       src/scrobbler.cpp                       \
                       src/uikeyhandler.cpp                    \
                       src/uiview.cpp                          \
                       src/util.cpp

LIBS                += -lncursesw                              \
                       -ltag

isEmpty(PREFIX) {
 PREFIX = /usr/local
}

program.path         = $$PREFIX/bin
macx: {
  program.files      = namp.app/Contents/MacOS/namp
}
unix:!macx {
  program.files      = namp
}

manpage.path         = $$PREFIX/share/man/man1
manpage.files        = res/namp.1

INSTALLS            += program manpage

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
  QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
}

