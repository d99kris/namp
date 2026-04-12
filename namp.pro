TARGET               = namp
TEMPLATE             = app
CONFIG              += c++11 release cmdline
QT                  += core multimedia

HEADERS              = src/audioplayer.h                       \
                       src/common.h                            \
                       src/log.h                               \
                       src/scrobbler.h                         \
                       src/spectrum.h                          \
                       src/uikeyhandler.h                      \
                       src/uiview.h                            \
                       src/util.h                              \
                       src/version.h

SOURCES              = src/audioplayer.cpp                     \
                       src/main.cpp                            \
                       src/log.cpp                             \
                       src/scrobbler.cpp                       \
                       src/spectrum.cpp                        \
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

# GUI support (default on, use CONFIG+=NO_GUI to disable)
!NO_GUI {
  DEFINES           += HAS_GUI
  INCLUDEPATH       += $$PWD/ext/cdgdeck
  HEADERS           += ext/cdgdeck/cdg.h                       \
                       src/cdgwindow.h
  SOURCES           += ext/cdgdeck/cdg.cpp                     \
                       src/cdgwindow.cpp
  HEADERS           += src/lyricsprovider.h                    \
                       src/lyricswindow.h
  SOURCES           += src/lyricsprovider.cpp                  \
                       src/lyricswindow.cpp
  RESOURCES         += res/namp.qrc
  QT                += network
  unix:!macx {
    QT              += widgets
  }
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

  QMAKE_INFO_PLIST   = res/InfoNoDock.plist

  !NO_GUI {
    QT              += widgets
    HEADERS           += src/macdock.h
    OBJECTIVE_SOURCES += src/macdock.mm
    LIBS              += -framework Cocoa
  }
}
