# spectre

Paste YouTube links, get audio in `~/Desktop/library`: **MP3 320 kbps**, **FLAC** or **WAV**.
Queue several links, cancel any of them; files are tagged with title and artist.

YouTube only serves lossy audio (Opus/AAC). FLAC and WAV keep that stream exactly with no further loss; MP3 320 re-encodes it.

## Install

**Linux (x86_64)**, needs glibc 2.38+ and SDL3 (Arch, Fedora 42+, Ubuntu 25.04+, Debian 13); the installer offers to install yt-dlp, ffmpeg and SDL3 if missing:
```sh
curl -fsSL https://raw.githubusercontent.com/333stack/spectre/main/install.sh | bash
```

**Windows 10/11 (x64)**, in PowerShell; downloads yt-dlp and ffmpeg if you don't have them:
```powershell
irm https://raw.githubusercontent.com/333stack/spectre/main/install.ps1 | iex
```

Uninstall: `./install.sh --uninstall` or `.\install.ps1 -Uninstall`.

Keep yt-dlp up to date: YouTube changes often break old versions.

## Build

Release binaries don't need kitui. Building from source needs kitui (private, ask for access) checked out next to this folder (`../kitui`), CMake, Ninja, and SDL3 on Linux.
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
scripts/package.sh   # release assets in dist/ (Linux tarball + Windows zip, cross-built with MinGW)
```

## License

Copyright 2026 Sandia. [PolyForm Noncommercial 1.0.0](LICENSE.md):

- You may use, modify and share spectre for any noncommercial purpose.
- You must always credit the author: keep the `Required Notice` line and the license with every copy, modified or not.
- Commercial use requires prior written permission. Contact [@333stack](https://github.com/333stack).

The bundled UI font is LGPL; see `NOTICE.txt` in each release. yt-dlp and ffmpeg are separate programs under their own licenses.
