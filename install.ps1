# spectre installer for Windows (x64). Per-user, no admin.
#   irm https://raw.githubusercontent.com/333stack/spectre/main/install.ps1 | iex
#   .\install.ps1 -Uninstall
#   .\install.ps1 -From .\spectre-windows-x64.zip   (install a local build)
param([switch]$Uninstall, [string]$From)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # Windows PowerShell 5.1 downloads crawl with the progress bar on
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor 3072  # TLS 1.2 on 5.1

$Repo = if ($env:SPECTRE_REPO) { $env:SPECTRE_REPO } else { '333stack/spectre' }
$Dir = Join-Path $env:LOCALAPPDATA 'Programs\spectre'
$Lnk = Join-Path ([Environment]::GetFolderPath('Programs')) 'spectre.lnk'

if (Get-Process spectre -ErrorAction SilentlyContinue) { Write-Host 'Close spectre first, then run this again.'; return }

if ($Uninstall) {
    Remove-Item -Recurse -Force $Dir, $Lnk -ErrorAction SilentlyContinue
    Write-Host 'spectre removed (your files in Desktop\library are untouched).'
    return  # not exit: under `irm | iex` that would close the user's window
}

$tmp = Join-Path ([IO.Path]::GetTempPath()) ("spectre-" + [guid]::NewGuid())
New-Item -ItemType Directory -Force $tmp, $Dir | Out-Null
try {
    $zip = Join-Path $tmp 'spectre.zip'
    if ($From) { Copy-Item $From $zip }
    else {
        Write-Host "Downloading spectre from github.com/$Repo ..."
        Invoke-WebRequest "https://github.com/$Repo/releases/latest/download/spectre-windows-x64.zip" -OutFile $zip -UseBasicParsing
    }
    Expand-Archive $zip $Dir -Force

    # yt-dlp and ffmpeg: use installed ones, otherwise put them next to spectre.exe
    # (Windows looks there first for yt-dlp; spectre passes --ffmpeg-location for ffmpeg).
    if (-not (Get-Command yt-dlp -ErrorAction SilentlyContinue)) {
        Write-Host 'Downloading yt-dlp ...'
        Invoke-WebRequest 'https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe' -OutFile (Join-Path $Dir 'yt-dlp.exe') -UseBasicParsing
    }
    if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue) -and -not (Test-Path (Join-Path $Dir 'ffmpeg.exe'))) {
        Write-Host 'Downloading ffmpeg (about 150 MB) ...'
        $ff = Join-Path $tmp 'ffmpeg.zip'
        Invoke-WebRequest 'https://github.com/yt-dlp/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip' -OutFile $ff -UseBasicParsing
        Expand-Archive $ff (Join-Path $tmp 'ff') -Force
        Get-ChildItem (Join-Path $tmp 'ff') -Recurse -Include ffmpeg.exe, ffprobe.exe | Copy-Item -Destination $Dir
    }

    $s = (New-Object -ComObject WScript.Shell).CreateShortcut($Lnk)
    $s.TargetPath = Join-Path $Dir 'spectre.exe'
    $s.WorkingDirectory = $Dir
    $s.Save()
    Write-Host "Installed spectre to $Dir. Open it from the Start menu."
} finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
