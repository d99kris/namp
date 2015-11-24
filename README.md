namp - ncurses audio media player
=================================
namp is a console MP3 player for Linux. 

![namp screenshot](/doc/namp-screenshot.png)

Features
========
- Supported file formats: MPEG 1.0/2.0/2.5 stream (layers 1, 2 and 3).
- Support for last.fm scrobbling
- Support for enqueueing tracks
- Infinite track (back) history
- Mouse support

Supported Platforms
===================
namp should work on most Linux systems where its dependencies are met;
libmpg123, libao, libasound, libcurl, libcrypto, libtag, libncursesw, libglib2 and help2man.

It has been tested on:
- Debian 8.2
- openSUSE 13.2
- Ubuntu 14.04

Dependencies
============

Ubuntu / Debian
---------------

    sudo apt-get install build-essential libglib2.0-dev libncursesw5-dev libmpg123-dev libao-dev libasound2-dev libtagc0-dev libcurl4-openssl-dev libssl-dev help2man

openSUSE
--------
Note: Some packages below are not available in standard repositories, but can be installed from for example packman-essentials.

    sudo zypper install make gcc glib2-devel ncurses-devel libmpg123-devel libao-devel alsa-devel libtag-devel libcurl-devel libopenssl-devel help2man

Installation
============
Configure and build:

    ./configure && make

Optionally install:

    sudo make install

Usage
=====
Refer to man-page or 'namp --help'.

License
=======
namp is distributed under GPLv2 license. See LICENSE file.

Keywords
========
console, linux, mp3 player, music player, ncurses, terminal.

