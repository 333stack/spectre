# spectre

Paste YouTube links, get audio in `~/Desktop/library`: **MP3 320 kbps**, **FLAC** or **WAV**.
Queue several links, cancel any of them; files are tagged with title and artist.

YouTube only serves lossy audio (Opus/AAC). FLAC and WAV keep that stream exactly with no further loss; MP3 320 re-encodes it.

## Install

**Linux (x86_64)**, needs glibc 2.38+ (Arch, Fedora 39+, Ubuntu 24.04+, Debian 13); the installer offers to install yt-dlp, ffmpeg and SDL3 if missing:
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

Needs [kitui](https://github.com/333stack/kitui) checked out next to this folder (`../kitui`), CMake, Ninja, and SDL3 on Linux.
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
scripts/package.sh   # release assets in dist/ (Linux tarball + Windows zip, cross-built with MinGW)
```

The bundled UI font is LGPL; see `NOTICE.txt` in each release.
