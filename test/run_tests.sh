#!/bin/bash
# Fuehrt alle Tests auf dem PC aus (ohne Hardware). Braucht: g++, python3 + playwright (Chromium) fuer den Browser-Test.
set -e
cd "$(dirname "$0")"
echo "== Gemeinsame Datei Common.h in beiden Sketches identisch? =="
diff ../kasse/Common.h ../schranken/Common.h && echo "ja"
echo "== Sketches fuer den PC bauen (Warnungen als Fehlerquelle: -Wall -Wextra) =="
./build_host.sh
echo "== Logik-Tests (42 Testfaelle + Zusatzfaelle) =="
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -I../kasse -I../schranken test_core.cpp -o build/test_core
./build/test_core
echo "== End-to-End-Test in Echtzeit (echte HTTP-Verbindungen zwischen den Sketches) =="
python3 test_e2e.py
if [ "$1" != "--no-browser" ]; then
  echo "== Browser-Test (Chromium) =="
  python3 test_ui.py
fi
echo "ALLE TESTS BESTANDEN"
