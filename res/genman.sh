#!/bin/bash

BASEDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/.."

PROGPATH="${BASEDIR}/namp"
if [ ! -f "${PROGPATH}" ]; then
  PROGPATH="${BASEDIR}/namp.app/Contents/MacOS/namp"
fi

if [ ! -f "${PROGPATH}" ]; then
  echo "Path to program 'namp' not found, exiting."
  exit 1
fi

help2man -n "ncurses audio media player" -N -o ${BASEDIR}/res/namp.1 ${PROGPATH}

exit ${?}

