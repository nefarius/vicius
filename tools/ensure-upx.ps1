<#
.SYNOPSIS
    Resolves or auto-installs UPX, then optionally packs a binary with it.

.PARAMETER Pack
    When specified, perform the pack/copy step after resolving UPX.

.PARAMETER InputFile
    Source binary to compress (required with -Pack).

.PARAMETER OutputFile
    Destination path for the compressed/copied binary (required with -Pack).

.PARAMETER Platform
    MSBuild platform string (e.g. x64, x86, ARM64). ARM64 skips UPX and copies verbatim.
#>
[CmdletBinding()]
param(
    [switch]  $Pack,
    [string]  $InputFile,
    [string]  $OutputFile,
    [string]  $Platform = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Helper: try to find upx on PATH and return the full path, or $null.
# ---------------------------------------------------------------------------
function Find-Upx {
    try {
        $cmd = Get-Command upx -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    } catch { }
    return $null
}

# ---------------------------------------------------------------------------
# Helper: attempt a single install method, return $true on success.
# ---------------------------------------------------------------------------
function Invoke-WingetInstall {
    Write-Host '[ensure-upx] Trying winget...'
    try {
        $result = winget install -e --id UPX.UPX `
            --accept-source-agreements --accept-package-agreements `
            --disable-interactivity 2>&1
        return ($LASTEXITCODE -eq 0)
    } catch { return $false }
}

function Invoke-ChocoInstall {
    Write-Host '[ensure-upx] Trying choco...'
    try {
        choco install upx -y 2>&1 | Out-Null
        return ($LASTEXITCODE -eq 0)
    } catch { return $false }
}

# ---------------------------------------------------------------------------
# Helper: download the latest UPX release zip from GitHub and extract it to
# a local cache directory. No admin rights required.
# ---------------------------------------------------------------------------
function Invoke-GitHubDownload {
    Write-Host '[ensure-upx] Trying GitHub download fallback...'
    $cacheDir = Join-Path $PSScriptRoot '.upx'
    $upxExe   = Join-Path $cacheDir 'upx.exe'

    if (Test-Path $upxExe) {
        Write-Host "[ensure-upx] Using cached UPX at $upxExe"
        return $upxExe
    }

    try {
        $apiUrl  = 'https://api.github.com/repos/upx/upx/releases/latest'
        $headers = @{ 'User-Agent' = 'ensure-upx.ps1' }
        $release = Invoke-RestMethod -Uri $apiUrl -Headers $headers

        # Pick the win64 asset
        $asset = $release.assets | Where-Object { $_.name -like '*win64*' } | Select-Object -First 1
        if (-not $asset) {
            Write-Warning '[ensure-upx] No win64 asset found in latest UPX release.'
            return $null
        }

        $zipPath = Join-Path $env:TEMP "upx_download.zip"
        Write-Host "[ensure-upx] Downloading $($asset.browser_download_url) ..."
        Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zipPath -UseBasicParsing

        if (-not (Test-Path $cacheDir)) { New-Item -ItemType Directory -Path $cacheDir | Out-Null }

        Expand-Archive -Path $zipPath -DestinationPath $cacheDir -Force
        Remove-Item $zipPath -ErrorAction SilentlyContinue

        # The archive contains a sub-folder; find the actual upx.exe
        $found = Get-ChildItem -Path $cacheDir -Recurse -Filter 'upx.exe' | Select-Object -First 1
        if (-not $found) {
            Write-Warning '[ensure-upx] upx.exe not found after extraction.'
            return $null
        }

        # Move it to the cache root for a stable path
        Move-Item -Path $found.FullName -Destination $upxExe -Force

        Write-Host "[ensure-upx] UPX downloaded to $upxExe"
        return $upxExe
    } catch {
        Write-Warning "[ensure-upx] GitHub download failed: $_"
        return $null
    }
}

# ---------------------------------------------------------------------------
# Resolve UPX (install if necessary).
# ---------------------------------------------------------------------------
$upxPath = Find-Upx

if (-not $upxPath) {
    Write-Host '[ensure-upx] UPX not found on PATH. Attempting auto-install...'

    if (Invoke-WingetInstall) {
        $upxPath = Find-Upx
    }

    if (-not $upxPath) {
        if (Invoke-ChocoInstall) {
            $upxPath = Find-Upx
        }
    }

    if (-not $upxPath) {
        $upxPath = Invoke-GitHubDownload
    }

    if (-not $upxPath) {
        Write-Error '[ensure-upx] Could not locate or install UPX. Aborting.'
        exit 1
    }

    # Ensure the resolved directory is on PATH for subsequent calls in this process.
    $upxDir = Split-Path $upxPath -Parent
    if ($env:PATH -notlike "*$upxDir*") {
        $env:PATH = "$upxDir;$env:PATH"
    }
}

Write-Host "[ensure-upx] Using UPX: $upxPath"

# ---------------------------------------------------------------------------
# Optional pack step.
# ---------------------------------------------------------------------------
if (-not $Pack) { exit 0 }

if (-not $InputFile -or -not $OutputFile) {
    Write-Error '[ensure-upx] -InputFile and -OutputFile are required when -Pack is specified.'
    exit 1
}

if ($Platform -eq 'ARM64') {
    Write-Host "[ensure-upx] ARM64 platform: copying $InputFile -> $OutputFile verbatim (UPX does not support ARM64)."
    Copy-Item -Path $InputFile -Destination $OutputFile -Force
    exit 0
}

# Remove the output file first (UPX requires it to not exist).
if (Test-Path $OutputFile) { Remove-Item $OutputFile -Force }

Write-Host "[ensure-upx] Packing: upx -9 -o `"$OutputFile`" `"$InputFile`""
& $upxPath -9 -o $OutputFile $InputFile
if ($LASTEXITCODE -ne 0) {
    Write-Error "[ensure-upx] UPX failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}
