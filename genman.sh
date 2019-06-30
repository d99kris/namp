#!/bin/bash

# genman.sh builds application and (re-)generates its man-page

PROGPATH="./namp"
if [[ "$(uname)" == "Darwin" ]]; then
  PROGPATH="./namp.app/Contents/MacOS/namp"
fi

qmake && make -s && \
help2man -n "ncurses audio media player" -N -o ./res/namp.1 ${PROGPATH}
exit ${?}

