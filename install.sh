#!/usr/bin/env bash
# Required Notice: Copyright 2026 STACK (https://github.com/333stack/spectre). Licensed under PolyForm Noncommercial 1.0.0, see LICENSE.md.
# spectre installer for Linux (x86_64). Installs for the current user only, no root needed for spectre itself.
#   curl -fsSL https://raw.githubusercontent.com/333stack/spectre/main/install.sh | bash
#   ./install.sh --uninstall
#   ./install.sh --from dist/spectre-linux-x86_64.tar.gz   (install a local build)
set -euo pipefail

REPO=${SPECTRE_REPO:-333stack/spectre}
BIN=${XDG_BIN_HOME:-$HOME/.local/bin}
DATA=${XDG_DATA_HOME:-$HOME/.local/share}
ASSET=spectre-linux-x86_64.tar.gz

say() { printf '\033[1m%s\033[0m\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }
ask() {  # y/N prompt that still works under `curl | bash` (stdin is the script)
    local a=""
    { : </dev/tty; } 2>/dev/null && read -rp "$1 [y/N] " a </dev/tty || true  # no terminal: answer no
    [[ $a == [yY]* ]]
}

if [ "${1:-}" = --uninstall ]; then
    rm -f "$BIN/spectre" "$DATA/applications/spectre.desktop"
    rm -rf "$DATA/doc/spectre"
    say "spectre removed (your files in ~/Desktop/library are untouched)."
    exit 0
fi

[ "$(uname -s)" = Linux ] || die "this installer is for Linux; on Windows use install.ps1"
[ "$(uname -m)" = x86_64 ] || die "only x86_64 builds are published"

# Runtime dependencies: yt-dlp + ffmpeg do the work, SDL3 draws the window.
missing=()
command -v yt-dlp >/dev/null || missing+=(yt-dlp)
command -v ffmpeg >/dev/null || missing+=(ffmpeg)
ldconfig -p 2>/dev/null | grep 'libSDL3\.so\.0' >/dev/null || missing+=(sdl3)  # not -q: early exit SIGPIPEs ldconfig, which pipefail reports as failure
if [ ${#missing[@]} -gt 0 ]; then
    cmd=""
    if command -v pacman >/dev/null; then cmd="sudo pacman -S --needed ${missing[*]}"
    elif command -v apt-get >/dev/null; then cmd="sudo apt-get install -y ${missing[*]/sdl3/libsdl3-0}"
    elif command -v dnf >/dev/null; then cmd="sudo dnf install -y ${missing[*]/sdl3/SDL3}"
    elif command -v zypper >/dev/null; then cmd="sudo zypper install -y ${missing[*]/sdl3/libSDL3-0}"
    fi
    say "Missing: ${missing[*]}"
    if [ -n "$cmd" ] && ask "Install them with: $cmd ?"; then
        $cmd
    else
        echo "Install them with your package manager, then run this again."
        [ -n "$cmd" ] && echo "  $cmd"
        exit 1
    fi
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
if [ "${1:-}" = --from ]; then
    [ -f "${2:-}" ] || die "--from needs a path to $ASSET"
    cp "$2" "$tmp/$ASSET"
else
    say "Downloading spectre from github.com/$REPO ..."
    curl -fL --progress-bar "https://github.com/$REPO/releases/latest/download/$ASSET" -o "$tmp/$ASSET" \
        || die "download failed (no release published yet?)"
fi
mkdir "$tmp/x"
tar -xzf "$tmp/$ASSET" -C "$tmp/x"
[ -f "$tmp/x/spectre" ] || die "archive has no spectre binary"

install -Dm755 "$tmp/x/spectre" "$BIN/spectre"
install -Dm644 "$tmp/x/spectre.desktop" "$DATA/applications/spectre.desktop"
install -Dm644 "$tmp/x/NOTICE.txt" "$DATA/doc/spectre/NOTICE.txt"
install -Dm644 "$tmp/x/LICENSE.md" "$DATA/doc/spectre/LICENSE.md"
sed -i "s|^Exec=.*|Exec=$BIN/spectre|" "$DATA/applications/spectre.desktop"  # works even if $BIN isn't on PATH
command -v update-desktop-database >/dev/null && update-desktop-database -q "$DATA/applications" || true

say "Installed spectre to $BIN/spectre"
echo "Open it from your app launcher, or run: spectre"
case ":$PATH:" in *":$BIN:"*) ;; *) echo "Note: $BIN is not on your PATH; add it to run 'spectre' from a terminal." ;; esac
