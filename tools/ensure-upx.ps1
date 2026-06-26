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
# ARM64 short-circuit: UPX does not support ARM64.
# Exit before attempting any UPX lookup or installation.
# ---------------------------------------------------------------------------
if ($Pack -and $Platform -eq 'ARM64') {
    if (-not $InputFile -or -not $OutputFile) {
        Write-Error '[ensure-upx] -InputFile and -OutputFile are required when -Pack is specified.'
        exit 1
    }
    Write-Host "[ensure-upx] ARM64 platform: copying $InputFile -> $OutputFile verbatim (UPX does not support ARM64)."
    Copy-Item -Path $InputFile -Destination $OutputFile -Force
    exit 0
}

# ---------------------------------------------------------------------------
# Helper: try to find upx on PATH and return the full path, or $null.
# ---------------------------------------------------------------------------
function Find-Upx {
    $cmd = Get-Command upx -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

# ---------------------------------------------------------------------------
# Helper: merge the machine + user registry PATH into the current process PATH
# so a freshly installed binary (winget/choco) becomes visible without losing
# any process-only entries injected by MSBuild, vcvars, or the CI agent.
# ---------------------------------------------------------------------------
function Update-ProcessPath {
    $machinePath = [System.Environment]::GetEnvironmentVariable('Path', 'Machine')
    $userPath    = [System.Environment]::GetEnvironmentVariable('Path', 'User')
    $combined    = @($env:PATH, $machinePath, $userPath) -join ';'
    # Deduplicate while preserving the original order (case-insensitive).
    $seen = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    $env:PATH = ($combined -split ';' |
        Where-Object { $_ -and $seen.Add($_) }) -join ';'
}

# ---------------------------------------------------------------------------
# Helpers: attempt a single install method, return $true on success.
# ---------------------------------------------------------------------------
function Invoke-WingetInstall {
    Write-Host '[ensure-upx] Trying winget...'
    try {
        winget install -e --id UPX.UPX `
            --accept-source-agreements --accept-package-agreements `
            --disable-interactivity 2>&1 | Out-Null
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
# Verifies SHA256 against the published sidecar before extracting.
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
        $release = Invoke-RestMethod -Uri $apiUrl -Headers $headers -TimeoutSec 60

        # Pick the win64 zip asset and its .sha256 sidecar.
        $asset = $release.assets |
            Where-Object { $_.name -like '*win64*.zip' } |
            Select-Object -First 1
        if (-not $asset) {
            Write-Warning '[ensure-upx] No win64 zip asset found in latest UPX release.'
            return $null
        }

        $sha256Asset = $release.assets |
            Where-Object { $_.name -eq "$($asset.name).sha256" } |
            Select-Object -First 1

        $zipPath = Join-Path $env:TEMP 'upx_download.zip'
        Write-Host "[ensure-upx] Downloading $($asset.browser_download_url) ..."
        Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zipPath -UseBasicParsing -TimeoutSec 120

        # Verify SHA256 when a sidecar checksum file is available.
        if ($sha256Asset) {
            Write-Host '[ensure-upx] Verifying SHA256 checksum...'
            $hashContent  = (Invoke-WebRequest -Uri $sha256Asset.browser_download_url `
                                -UseBasicParsing -TimeoutSec 30).Content.Trim()
            # Format is either "<hash>  <filename>" or just "<hash>".
            $expectedHash = ($hashContent -split '\s+')[0].ToUpperInvariant()
            $actualHash   = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
            if ($actualHash -ne $expectedHash) {
                Remove-Item $zipPath -ErrorAction SilentlyContinue
                Write-Warning "[ensure-upx] SHA256 mismatch: expected $expectedHash, got $actualHash. Aborting download."
                return $null
            }
            Write-Host "[ensure-upx] SHA256 OK ($actualHash)."
        } else {
            Write-Warning '[ensure-upx] No .sha256 sidecar found for this release; skipping integrity check.'
        }

        if (-not (Test-Path $cacheDir)) { New-Item -ItemType Directory -Path $cacheDir | Out-Null }

        Expand-Archive -Path $zipPath -DestinationPath $cacheDir -Force
        Remove-Item $zipPath -ErrorAction SilentlyContinue

        # The archive contains a sub-folder; locate the actual upx.exe.
        $found = Get-ChildItem -Path $cacheDir -Recurse -Filter 'upx.exe' | Select-Object -First 1
        if (-not $found) {
            Write-Warning '[ensure-upx] upx.exe not found after extraction.'
            return $null
        }

        # Move to the cache root for a stable path on subsequent calls.
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
        Update-ProcessPath
        $upxPath = Find-Upx
    }

    if (-not $upxPath) {
        if (Invoke-ChocoInstall) {
            Update-ProcessPath
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

    # Ensure the resolved directory is on PATH for subsequent calls in this
    # process (most relevant when using the GitHub cache path).
    $upxDir       = Split-Path $upxPath -Parent
    $pathSegments = $env:PATH -split ';'
    if (-not ($pathSegments | Where-Object { $_ -ieq $upxDir })) {
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

# Remove the output file first (UPX requires it to not exist).
if (Test-Path $OutputFile) { Remove-Item $OutputFile -Force }

Write-Host "[ensure-upx] Packing: upx -9 -o `"$OutputFile`" `"$InputFile`""
& $upxPath -9 -o $OutputFile $InputFile
if ($LASTEXITCODE -ne 0) {
    Write-Error "[ensure-upx] UPX failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}
