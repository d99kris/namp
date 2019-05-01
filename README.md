namp - ncurses audio media player
=================================

| **Linux + Mac** |
|-----------------|
| [![Build status](https://travis-ci.org/d99kris/namp.svg?branch=master)](https://travis-ci.org/d99kris/namp) |

namp is a console-based MP3 player for Linux and macOS, implemented in C++ / Qt.
For systems which do not have Qt available, a lite version of namp 
[namp-lite](https://github.com/d99kris/namp-lite) (Linux only) is available.

![linux screenshot](/res/namp-linux-screenshot.png) 


Usage
=====
Usage:

    namp OPTION
    namp PATH...

Command-line Options:

    -h, --help        display this help and exit
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
    pgup              playlist previous page
    pgdn              playlist next page
    ENTER             play selected track
    TAB               toggle main window / playlist focus
    s                 toggle shuffle on/off

Supported Platforms
===================
namp is primarily developed and tested on Linux, but basic functionality should work in macOS / OS X as well. Current version has been tested on:

- OS X El Capitan 10.11
- Ubuntu 16.04 LTS

Linux / Ubuntu
==============

**Dependencies**

    sudo apt install libncursesw5-dev libtag1-dev qt5-default qt5-qmake qtmultimedia5-dev libqt5multimedia5-plugins ubuntu-restricted-extras

**Source**

    git clone https://github.com/d99kris/namp && cd namp

**Build**

    qmake && make -s

**Install**

    sudo make -s install

macOS
=====

**Dependencies**

    brew install ncurses taglib
    brew install qt5 && brew link --force qt5

**Source**

    git clone https://github.com/d99kris/namp && cd namp

**Build**

    qmake && make -s

**Installation**

    cp -r namp.app /Applications/namp.app

Optionally add an alias to your ~/.bash_profile like this:

    alias namp='/Applications/namp.app/Contents/MacOS/namp'

License
=======
namp is distributed under GPLv2 license. See LICENSE file.

Keywords
========
command line, console, linux, macos, mp3 player, music player, ncurses, terminal.

