#!/bin/bash
# Baut beide Sketches fuer den PC (mit den Ersatz-Headern in test/stubs) - zum Testen ohne Hardware.
set -e
cd "$(dirname "$0")/.."
mkdir -p test/build
FLAGS="-std=c++17 -Wall -Wextra -Wno-unused-parameter -Itest/stubs -include Arduino.h"
g++ $FLAGS -Ikasse -x c++ kasse/kasse.ino -c -o test/build/kasse.o
g++ $FLAGS -x c++ test/host_main.cpp -c -o test/build/main.o
g++ test/build/kasse.o test/build/main.o -o test/build/kasse_host
g++ $FLAGS -Ischranken -DKASSE_HOST='"127.0.0.2"' -DKASSE_PORT=80 -x c++ schranken/schranken.ino -c -o test/build/schranken.o
g++ test/build/schranken.o test/build/main.o -o test/build/schranken_host
echo "Build OK"
