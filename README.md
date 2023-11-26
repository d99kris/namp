namp
====

| **Linux** | **Mac** |
|-----------|---------|
| [![Linux](https://github.com/d99kris/namp/workflows/Linux/badge.svg)](https://github.com/d99kris/namp/actions?query=workflow%3ALinux) | [![macOS](https://github.com/d99kris/namp/workflows/macOS/badge.svg)](https://github.com/d99kris/namp/actions?query=workflow%3AmacOS) |

namp is a terminal-based MP3 player for Linux and macOS, implemented in C++ / Qt.
For systems which do not have Qt available, a lite version of namp 
[namp-lite](https://github.com/d99kris/namp-lite) (Linux only) is available.

![screenshot](/res/namp-screenshot.png) 


Usage
=====
Usage:

    namp OPTION
    namp PATH...

Command-line Options:

    -h, --help        display this help and exit
    -s, --setup       setup last.fm scrobbling account
    -v, --version     output version information and exit
    PATH              file or directory to add to playlist

Command-line Examples:

    namp ~/Music      play all files in ~/Music
    namp hello.mp3    play hello.mp3

Interactive Commands:

    z                 previous track
    x                 play
    c                 pause
    v                 stop
    b                 next track
    q,ESC             quit
    /,j               find
    up,+              volume up
    down,-            volume down
    left              skip/fast backward
    right             skip/fast forward
    home              playlist top
    end               playlist end
    pgup              playlist previous page
    pgdn              playlist next page
    ENTER             play selected track
    TAB               toggle main window / playlist focus
    s                 toggle shuffle on/off

Supported Platforms
===================
namp is primarily developed and tested on Linux, but basic functionality should work in macOS / OS X as well. Current version has been tested on:

- macOS Big Sur 11.0
- Ubuntu 20.04 LTS

Linux / Ubuntu
==============

**Dependencies**

    sudo apt install libncursesw5-dev libtag1-dev qt5-default qt5-qmake qtmultimedia5-dev libqt5multimedia5-plugins ubuntu-restricted-extras

**Source**

    git clone https://github.com/d99kris/namp && cd namp

**Build**

    mkdir -p build && cd build && qmake .. && make -s

**Install**

    sudo make -s install

macOS
=====

**Dependencies**

    brew install ncurses taglib gnu-sed
    brew install qt

**Source**

    git clone https://github.com/d99kris/namp && cd namp

**Build**

    mkdir -p build && cd build && qmake .. && make -s

**Install**

    make -s install

License
=======
namp is distributed under GPLv2 license. See LICENSE file.

Keywords
========
command line, console-based, linux, macos, mp3 player, music player, ncurses, terminal-based.

