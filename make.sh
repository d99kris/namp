#!/usr/bin/env bash

# make.sh
#
# Copyright (C) 2020-2022 Kristofer Berggren
# All rights reserved.
#
# See LICENSE for redistribution information.

# exiterr
exiterr()
{
  >&2 echo "${1}"
  exit 1
}

# process arguments
DEPS="0"
BUILD="0"
DEVBUILD="0"
TESTS="0"
DOC="0"
INSTALL="0"
case "${1%/}" in
  deps)
    DEPS="1"
    ;;

  build)
    BUILD="1"
    ;;

  devbuild)
    DEVBUILD="1"
    ;;

  test*)
    BUILD="1"
    TESTS="1"
    ;;

  doc)
    BUILD="1"
    DOC="1"
    ;;

  install)
    BUILD="1"
    INSTALL="1"
    ;;

  all)
    DEPS="1"
    BUILD="1"
    DEVBUILD="1"
    TESTS="1"
    DOC="1"
    INSTALL="1"
    ;;

  *)
    echo "usage: make.sh <deps|build|tests|doc|install|all>"
    echo "  deps      - install project dependencies"
    echo "  build     - perform build"
    echo "  devbuild  - perform dev build"
    echo "  tests     - perform build and run tests"
    echo "  doc       - perform build and generate documentation"
    echo "  install   - perform build and install"
    echo "  all       - perform all actions above"
    exit 1
    ;;
esac

# deps
if [[ "${DEPS}" == "1" ]]; then
  OS="$(uname)"
  if [ "${OS}" == "Linux" ]; then
    DISTRO="$(lsb_release -i | awk -F':\t' '{print $2}')"
    if [[ "${DISTRO}" == "Ubuntu" ]]; then
      sudo apt -y update && \
      sudo apt -y install libncursesw5-dev libtag1-dev qt5-default qt5-qmake qtmultimedia5-dev libqt5multimedia5-plugins ubuntu-restricted-extras || exiterr "deps failed (linux), exiting."
    else
      exiterr "deps failed (unsupported linux distro ${DISTRO}), exiting."
    fi
  elif [ "${OS}" == "Darwin" ]; then
    brew install ncurses taglib gnu-sed qt || exiterr "deps failed (mac), exiting."
  else
    exiterr "deps failed (unsupported os ${OS}), exiting."
  fi
fi

# build
if [[ "${BUILD}" == "1" ]]; then
  OS="$(uname)"
  MAKEARGS=""
  if [ "${OS}" == "Linux" ]; then
    MAKEARGS="-j$(nproc)"
  elif [ "${OS}" == "Darwin" ]; then
    MAKEARGS="-j$(sysctl -n hw.ncpu)"
  fi
  mkdir -p build && cd build && qmake .. && make ${MAKEARGS} && cd .. || exiterr "build failed, exiting."
fi

# devbuild
if [[ "${DEVBUILD}" == "1" ]]; then
  OS="$(uname)"
  MAKEARGS=""
  if [ "${OS}" == "Linux" ]; then
    MAKEARGS="-j$(nproc)"
  elif [ "${OS}" == "Darwin" ]; then
    MAKEARGS="-j$(sysctl -n hw.ncpu)"
  fi
  mkdir -p devbuild && cd devbuild && qmake CONFIG+=DEVBUILD .. && make ${MAKEARGS} && cd .. || exiterr "devbuild failed, exiting."
fi

# tests
if [[ "${TESTS}" == "1" ]]; then
  true || exiterr "tests failed, exiting."
fi

# doc
if [[ "${DOC}" == "1" ]]; then
  if [[ -x "$(command -v help2man)" ]]; then
    if [[ "$(uname)" == "Darwin" ]]; then
      SED="gsed -i"
    else
      SED="sed -i"
    fi

    PROGPATH="./build/namp"
    help2man -n "ncurses audio media player" -N -o ./res/namp.1 ${PROGPATH} && ${SED} "s/\.\\\\\" DO NOT MODIFY THIS FILE\!  It was generated by help2man.*/\.\\\\\" DO NOT MODIFY THIS FILE\!  It was generated by help2man./g" ./res/namp.1 || exiterr "doc failed, exiting."
  fi
fi

# install
if [[ "${INSTALL}" == "1" ]]; then
  OS="$(uname)"
  if [ "${OS}" == "Linux" ]; then
    cd build && sudo make install && cd .. || exiterr "install failed (linux), exiting."
  elif [ "${OS}" == "Darwin" ]; then
    cd build && make install && cd .. || exiterr "install failed (mac), exiting."
  else
    exiterr "install failed (unsupported os ${OS}), exiting."
  fi
fi

# exit
exit 0
