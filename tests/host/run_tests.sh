#!/usr/bin/env bash
# Builds and runs the host unit tests for the hardware independent modules.
set -euo pipefail

cd "$(dirname "$0")/../.."

CXX="${CXX:-g++}"
OUT="tests/host/build"
mkdir -p "$OUT"

echo "== compiling =="
"$CXX" -std=gnu++17 -O1 -g -Wall -Wextra -Iinclude -Isrc \
  tests/host/test_core.cpp src/core/*.cpp \
  -o "$OUT/test_core"

"$CXX" -std=gnu++17 -O1 -g -Wall -Wextra -Iinclude -Isrc \
  tests/host/check_wavs.cpp src/core/*.cpp \
  -o "$OUT/check_wavs"

echo "== running core unit tests =="
"$OUT/test_core"

echo
echo "== running sample pack check =="
if [ -d sdcard/DRUM ]; then
  "$OUT/check_wavs" sdcard/DRUM
else
  echo "sdcard/DRUM not found - run python3 tools/make_test_wavs.py first"
  exit 1
fi
