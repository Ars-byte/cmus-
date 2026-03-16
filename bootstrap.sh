#!/usr/bin/env bash
# bootstrap.sh — build CMUS++ (modular layout)
set -e

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra"

# Detect platform libs
case "$(uname -s)" in
  Linux)
    LIBS="-lasound -lsndfile"
    ;;
  Darwin)
    LIBS="-lsndfile -framework AudioToolbox -framework CoreAudio"
    ;;
  MINGW*|CYGWIN*|MSYS*)
    LIBS="-lsndfile -lwinmm"
    ;;
  *)
    echo "Unsupported platform"; exit 1 ;;
esac

echo "Building CMUS++ …"
$CXX $CXXFLAGS \
    main.cpp \
    -I. \
    $LIBS \
    -o cmuspp

echo "Done → ./cmuspp"
echo ""
echo "Custom themes: drop *.xml files in ./themes/ and restart."
