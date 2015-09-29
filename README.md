namp - ncurses audio media player
=================================
namp is a minimalistic command-line based audio player.

Supported Platforms
===================
namp should work on most Linux systems with the following libraries present;
libmpg123, libao, libasound, libcurl, libcrypto, libtag_c, libncurses and 
libglib-2.0

It has been tested on:
- Ubuntu 14.04 LTS

Dependencies
============
Ubuntu:

    sudo apt-get install libglib2.0-dev libncursesw5-dev libmpg123-dev libao-dev libasound2-dev libtagc0-dev libcurl4-openssl-dev

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
namp is distributed under GPLv2 license. See LICENSE file. Its dependencies 
are licensed per the following table:

| Library      | License           |
| ------------ | ----------------- |
| libmpg123    | LGPL v2.1         |
| libao        | GPL v2.0          |
| libasound    | LGPL              |
| libcurl      | MIT/X derivative  |
| libcrypto    | BSD derivative    |
| libtag_c     | LGPL              |
| libncurses   | MIT/X derivative  |
| libglib-2.0  | LGPL              |

Keywords
========
command line interface, mp3 player, linux.

