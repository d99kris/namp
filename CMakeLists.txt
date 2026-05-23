cmake_minimum_required(VERSION 3.16)
project(namp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(NO_GUI "Disable GUI support (cdg/lyrics windows)" OFF)
option(DEVBUILD "Build dev variant as nmp123 using dev/ sources" OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(QT_COMPONENTS Core Multimedia)
if(NOT NO_GUI)
    list(APPEND QT_COMPONENTS Network Widgets)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${QT_COMPONENTS})
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${QT_COMPONENTS})

if(DEVBUILD)
    set(TARGET_NAME nmp123)
else()
    set(TARGET_NAME namp)
endif()

set(SOURCES
    src/audioplayer.cpp
    src/main.cpp
    src/log.cpp
    src/scrobbler.cpp
    src/spectrum.cpp
    src/util.cpp
    src/audioplayer.h
    src/common.h
    src/log.h
    src/scrobbler.h
    src/spectrum.h
    src/uikeyhandler.h
    src/uiview.h
    src/util.h
    src/version.h
)

if(DEVBUILD)
    list(APPEND SOURCES dev/uikeyhandler.cpp dev/uiview.cpp)
else()
    list(APPEND SOURCES src/uikeyhandler.cpp src/uiview.cpp)
endif()

if(NOT NO_GUI)
    list(APPEND SOURCES
        ext/cdgdeck/cdg.cpp
        ext/cdgdeck/cdg.h
        src/cdgwindow.cpp
        src/cdgwindow.h
        src/lyricsprovider.cpp
        src/lyricsprovider.h
        src/lyricswindow.cpp
        src/lyricswindow.h
        res/namp.qrc
    )
endif()

if(APPLE AND NOT NO_GUI)
    enable_language(OBJCXX)
    list(APPEND SOURCES src/macdock.h src/macdock.mm)
endif()

add_executable(${TARGET_NAME} ${SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE OFF
    WIN32_EXECUTABLE OFF
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(NOT NO_GUI)
    target_compile_definitions(${TARGET_NAME} PRIVATE HAS_GUI)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ext/cdgdeck
    )
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Multimedia
    ncursesw
    tag
)

if(NOT NO_GUI)
    target_link_libraries(${TARGET_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Widgets
    )
endif()

if(UNIX AND NOT APPLE)
    target_include_directories(${TARGET_NAME} PRIVATE
        /usr/include/taglib
        /usr/include/ncursesw
    )
endif()

if(APPLE)
    target_include_directories(${TARGET_NAME} PRIVATE
        /usr/local/opt/ncurses/include
        /usr/local/opt/taglib/include/taglib
        /opt/homebrew/opt/ncurses/include
        /opt/homebrew/opt/taglib/include/taglib
    )
    target_link_directories(${TARGET_NAME} PRIVATE
        /usr/local/opt/ncurses/lib
        /usr/local/opt/taglib/lib
        /opt/homebrew/opt/ncurses/lib
        /opt/homebrew/opt/taglib/lib
    )
    target_compile_definitions(${TARGET_NAME} PRIVATE _XOPEN_SOURCE_EXTENDED=1)
    target_link_options(${TARGET_NAME} PRIVATE
        "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/res/InfoNoDock.plist"
    )
    if(NOT NO_GUI)
        target_link_libraries(${TARGET_NAME} PRIVATE "-framework Cocoa")
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES res/namp.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
