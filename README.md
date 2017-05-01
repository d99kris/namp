namp - ncurses audio media player
=================================
namp is a console MP3 player for Linux and macOS.

![namp screenshot](/res/namp-screenshot.png)

Usage
=====
Usage: namp OPTION
   or: namp PATH...

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

Dependencies
============

Ubuntu
------

    sudo apt install libncursesw5-dev qt5-default qt5-qmake qtmultimedia5-dev libqt5multimedia5-plugins libtag1-dev
    sudo apt install ubuntu-restricted-extras

macOS
-----

    brew install ncurses taglib

Installation
============
Generate Makefile and build:

    qmake && make -s

Optionally install:

    sudo make -s install

Implementation
==============
namp is implemented in C++ / Qt. The original implementation of namp was in C, and that code is available
in branch [v1](/tree/v1) for those interested.

License
=======
namp is distributed under GPLv2 license. See LICENSE file.

Keywords
========
console, linux, macos, mp3 player, music player, ncurses, terminal.

