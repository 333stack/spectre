#!/usr/bin/env bash
# Required Notice: Copyright 2026 STACK (https://github.com/333stack/spectre). Licensed under PolyForm Noncommercial 1.0.0, see LICENSE.md.
# Build release binaries and pack the GitHub release assets into dist/:
#   spectre-linux-x86_64.tar.gz  spectre-windows-x64.zip
set -euo pipefail
cd "$(dirname "$0")/.."
notice=../kitui/third_party/fonts/NOTICE.txt  # baked font is LGPL: ship its notice with every binary

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build --target spectre
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../kitui/cmake/mingw-w64.cmake >/dev/null
cmake --build build-win --target spectre

rm -rf dist && mkdir -p dist/linux dist/windows
cp build/spectre spectre.desktop dist/linux/
cp build-win/spectre.exe dist/windows/
cp "$notice" dist/linux/NOTICE.txt
cp "$notice" dist/windows/NOTICE.txt
cp LICENSE.md dist/linux/ && cp LICENSE.md dist/windows/
strip dist/linux/spectre
x86_64-w64-mingw32-strip dist/windows/spectre.exe
tar -C dist/linux -czf dist/spectre-linux-x86_64.tar.gz .
(cd dist/windows && python3 -m zipfile -c ../spectre-windows-x64.zip spectre.exe NOTICE.txt LICENSE.md)
rm -rf dist/linux dist/windows
(cd dist && sha256sum ./*.tar.gz ./*.zip > SHA256SUMS)
ls -la dist
