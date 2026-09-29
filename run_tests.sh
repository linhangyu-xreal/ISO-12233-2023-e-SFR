#!/bin/sh
set -eu

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists opencv4; then
    g++ -std=c++17 -O2 -DSFRMAT5_HAVE_OPENCV $(pkg-config --cflags opencv4) -Ithird_party/eigen-3.4.0 -Ithird_party cpp/sfrmat5.cpp cpp/test_sfrmat5.cpp -o /tmp/test_sfrmat5 $(pkg-config --libs opencv4)
else
    g++ -std=c++17 -O2 -Ithird_party/eigen-3.4.0 -Ithird_party cpp/sfrmat5.cpp cpp/test_sfrmat5.cpp -o /tmp/test_sfrmat5
fi
/tmp/test_sfrmat5
