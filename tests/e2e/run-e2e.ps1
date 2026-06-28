#!/usr/bin/env pwsh
#Requires -Version 7.0
<#
.SYNOPSIS
    Runs the vicius E2E test suite against a locally started example server.

.DESCRIPTION
    Each scenario copies the appropriate updater binary to an isolated temp dir,
    invokes it with --silent-update (headless mode) and --force-local-version
    to guarantee a deterministic "installed vs. available" comparison, then
    asserts the process exit code against the expected value.

    Exit codes tested:
        NV_S_UPDATE_FINISHED = 203
        NV_S_UP_TO_DATE      = 202
        NV_S_SELF_UPDATER    = 201
        NV_E_SERVER_RESPONSE = 104
        NV_E_SIGNATURE_INVALID = 116

.PARAMETER MainBin
    Path to the main E2E updater binary (e2e_Main_Updater.exe), built with
    NV_FLAGS_ALLOW_HTTP_DOWNLOAD and pointing at localhost:5200.

.PARAMETER SigBin
    Path to the signed-manifest E2E updater binary (e2eSig_Sig_Updater.exe),
    same as MainBin plus NV_MANIFEST_PUBLIC_KEY compiled in.

.PARAMETER ProdBin
    Path to the standard release updater (example_Demo_Updater.exe) built WITHOUT
    NV_FLAGS_ALLOW_HTTP_DOWNLOAD, used for the HttpRejected negative-control test.

.PARAMETER ArtifactsDir
    E2E_ARTIFACTS_DIR: directory pre-populated by CI with payload.zip, setup.exe,
    updater_selfupdate.exe, SignedManifest/, and TamperedManifest/ subdirectories.

.PARAMETER ServerDir
    Path to the examples/server project directory (for 'dotnet run').

.PARAMETER LogDir
    Directory where per-scenario log files are written. Defaults to a 'e2e-logs'
    subdirectory next to this script.
#>
param(
    [Parameter(Mandatory)]
    [string] $MainBin,

    [Parameter(Mandatory)]
    [string] $SigBin,

    [Parameter(Mandatory)]
    [string] $ProdBin,

    [Parameter(Mandatory)]
    [string] $ArtifactsDir,

    [Parameter(Mandatory)]
    [string] $ServerDir,

    [string] $LogDir = (Join-Path $PSScriptRoot 'e2e-logs')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Write-Header([string] $title) {
    $line = '─' * 60
    Write-Host "`n$line"
    Write-Host "  $title"
    Write-Host $line
}

function Start-E2EServer {
    Write-Host 'Starting example server...'
    $env:VICIUS_E2E        = '1'
    $env:E2E_ARTIFACTS_DIR = $ArtifactsDir
    $env:ASPNETCORE_URLS   = 'http://localhost:5200'
    $env:ASPNETCORE_ENVIRONMENT = 'Development'

    # Run the pre-built server DLL directly; avoids 'dotnet run' profile
    # ambiguity and works reliably with -RedirectStandard* on PS 7.
    $serverDll = Join-Path $ServerDir 'bin' 'Release' 'net10.0' 'server.dll'

    $job = Start-Process `
        -FilePath 'dotnet' `
        -ArgumentList $serverDll `
        -PassThru `
        -NoNewWindow `
        -RedirectStandardOutput (Join-Path $LogDir 'server.stdout.txt') `
        -RedirectStandardError  (Join-Path $LogDir 'server.stderr.txt')

    # Wait for the readiness probe (GET api/vicius/master/schema.json → 200)
    $readyUrl = 'http://localhost:5200/api/vicius/master/schema.json'
    $deadline = (Get-Date).AddSeconds(60)
    $ready = $false
    Write-Host "  Waiting for server readiness at $readyUrl ..."
    while ((Get-Date) -lt $deadline) {
        if ($job.HasExited) {
            $stderr = Get-Content (Join-Path $LogDir 'server.stderr.txt') -Raw -ErrorAction SilentlyContinue
            throw "Example server exited unexpectedly (exit code $($job.ExitCode)).`nRecent stderr:`n$stderr"
        }
        try {
            $r = Invoke-WebRequest -Uri $readyUrl -UseBasicParsing -TimeoutSec 2 -ErrorAction Stop
            if ($r.StatusCode -eq 200) { $ready = $true; break }
        } catch { }
        Start-Sleep -Milliseconds 500
    }

    if (-not $ready) {
        Stop-Process -Id $job.Id -Force -ErrorAction SilentlyContinue
        throw 'Example server did not become ready within 60 seconds.'
    }

    Write-Host "  Server is ready (PID $($job.Id))."
    return $job
}

function Stop-E2EServer([System.Diagnostics.Process] $job) {
    if ($job -and -not $job.HasExited) {
        Write-Host 'Stopping example server...'
        Stop-Process -Id $job.Id -Force -ErrorAction SilentlyContinue
        $job.WaitForExit(5000) | Out-Null
    }
}

function Invoke-Scenario {
    param(
        [string]      $Name,
        [string]      $SourceBin,                  # full path to the binary to copy
        [string]      $ExeName,                    # name the binary is copied to (determines tenant path)
        [string]      $LocalVersion    = '',        # --force-local-version value; empty = omit the flag
        [int]         $ExpectedExit,
        [bool]        $SkipSelfUpdate  = $true,
        [bool]        $NeedsInstall    = $false,
        [bool]        $UseLocalVersion = $true,     # false = omit --force-local-version (server-driven detection)
        [hashtable]   $Sidecar         = $null,    # optional: @{ name = "..."; content = "..." }
        [scriptblock] $PreScenario     = $null,    # runs before the binary is invoked
        [scriptblock] $PostScenario    = $null     # runs in finally, even on failure
    )

    Write-Header "Scenario: $Name (expect exit $ExpectedExit)"

    $workDir = Join-Path $env:TEMP "vicius-e2e-$Name-$(New-Guid)"
    New-Item -ItemType Directory -Path $workDir | Out-Null

    $exePath = Join-Path $workDir $ExeName
    Copy-Item -Path $SourceBin -Destination $exePath

    if ($null -ne $Sidecar) {
        $sidecarPath = Join-Path $workDir $Sidecar.Name
        $Sidecar.Content | Set-Content -Path $sidecarPath -Encoding utf8
    }

    $logFile = Join-Path $LogDir "$Name.log"

    try {
        if ($null -ne $PreScenario) {
            Write-Host "  Running pre-scenario hook..."
            & $PreScenario
        }

        # ── Optional install step (extracts DLL to Alternate Data Stream) ────
        if ($NeedsInstall) {
            Write-Host "  Running --install step..."
            $installProc = Start-Process `
                -FilePath $exePath `
                -ArgumentList '--install', '--no-autostart', '--no-scheduled-task',
                              '--log-to-file', $logFile, '--log-level', 'debug' `
                -Wait -PassThru -NoNewWindow
            $installCode = $installProc.ExitCode
            Write-Host "  Install step exit code: $installCode"
            if ($installCode -ne 200) {
                Write-Warning "  Install step returned $installCode (expected 200); skipping test."
                return @{ Name = $Name; Passed = $false; Expected = $ExpectedExit; Got = $installCode;
                          Note = "install step failed" }
            }
        }

        # ── Main test invocation ─────────────────────────────────────────────
        $args = @(
            '--silent-update',
            '--ignore-busy-state',
            '--log-to-file', $logFile,
            '--log-level', 'debug'
        )
        # Only pass --force-local-version when the scenario uses FixedVersion detection.
        # Server-driven detection scenarios (registry, file version) must omit it so the
        # manifest's shared.detection block is honoured.
        if ($UseLocalVersion -and $LocalVersion -ne '') {
            $args = @('--force-local-version', $LocalVersion) + $args
        }
        if ($SkipSelfUpdate) { $args += '--skip-self-update' }

        Write-Host "  Running: $ExeName $args"
        $proc = Start-Process `
            -FilePath $exePath `
            -ArgumentList $args `
            -Wait -PassThru -NoNewWindow
        $got = $proc.ExitCode

        # ── For the SelfUpdate scenario: allow the self-updater DLL to finish ─
        if ($Name -eq 'SelfUpdate') {
            Write-Host "  Waiting 15s for self-updater DLL to complete..."
            Start-Sleep -Seconds 15
            # Verify the binary was restored by the DLL after Authenticode failure
            if (-not (Test-Path $exePath)) {
                Write-Host "  WARN: binary not found after DLL run; backup restoration may have failed."
            } else {
                Write-Host "  Binary present at $exePath (DLL restored it correctly)."
            }
        }

        $passed = ($got -eq $ExpectedExit)
        $status = if ($passed) { 'PASS' } else { 'FAIL' }
        Write-Host "  $status  exit=$got  expected=$ExpectedExit"

        if (-not $passed) {
            Write-Host "  Log tail:"
            if (Test-Path $logFile) {
                Get-Content $logFile -Tail 30 | ForEach-Object { Write-Host "    $_" }
            }
        }

        return @{ Name = $Name; Passed = $passed; Expected = $ExpectedExit; Got = $got }
    }
    finally {
        Remove-Item -Path $workDir -Recurse -Force -ErrorAction SilentlyContinue
        if ($null -ne $PostScenario) {
            Write-Host "  Running post-scenario hook..."
            & $PostScenario
        }
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

New-Item -ItemType Directory -Path $LogDir -Force | Out-Null

# Validate inputs
foreach ($p in @($MainBin, $SigBin, $ProdBin, $ArtifactsDir, $ServerDir)) {
    if (-not (Test-Path $p)) { throw "Required path not found: $p" }
}

$serverJob = $null
$results   = [System.Collections.Generic.List[hashtable]]::new()

try {
    # Build the server first so 'dotnet run --no-build' is fast
    Write-Host 'Building example server...'
    & dotnet build $ServerDir -c Release --nologo -v quiet
    if ($LASTEXITCODE -ne 0) { throw "dotnet build failed with exit $LASTEXITCODE" }

    $serverJob = Start-E2EServer

    # ── HttpRejected sidecar content ────────────────────────────────────────
    # The production binary's config file can override serverUrlTemplate.
    # The binary lacks NV_FLAGS_ALLOW_HTTP_DOWNLOAD so http://localhost is rejected.
    $httpRejectedSidecar = @{
        Name    = 'example_Demo_Updater.json'
        Content = '{"instance":{"serverUrlTemplate":"http://localhost:5200/api/{}/updates.json"}}'
    }

    # ── Scenario table ───────────────────────────────────────────────────────
    $scenarios = @(
        @{
            Name           = 'HappyZip'
            SourceBin      = $MainBin
            ExeName        = 'e2e_HappyZip_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 203
            SkipSelfUpdate = $true
        },
        @{
            Name           = 'HappyExe'
            SourceBin      = $MainBin
            ExeName        = 'e2e_HappyExe_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 203
            SkipSelfUpdate = $true
        },
        @{
            Name           = 'UpToDate'
            SourceBin      = $MainBin
            ExeName        = 'e2e_UpToDate_Updater.exe'
            LocalVersion   = '2.0.0'
            ExpectedExit   = 202
            SkipSelfUpdate = $true
        },
        @{
            Name           = 'ChecksumMismatch'
            SourceBin      = $MainBin
            ExeName        = 'e2e_ChecksumMismatch_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 116
            SkipSelfUpdate = $true
        },
        @{
            Name           = 'ServerError'
            SourceBin      = $MainBin
            ExeName        = 'e2e_ServerError_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 104
            SkipSelfUpdate = $true
        },
        @{
            Name           = 'HttpRejected'
            SourceBin      = $ProdBin
            ExeName        = 'example_Demo_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 104
            SkipSelfUpdate = $true
            Sidecar        = $httpRejectedSidecar
        },
        @{
            Name           = 'SelfUpdate'
            SourceBin      = $MainBin
            ExeName        = 'e2e_SelfUpdate_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 201
            SkipSelfUpdate = $false
            NeedsInstall   = $true
        },
        @{
            Name           = 'SignedManifest'
            SourceBin      = $SigBin
            ExeName        = 'e2eSig_SignedManifest_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 203
            SkipSelfUpdate = $true
        },
        @{
            Name           = 'TamperedManifest'
            SourceBin      = $SigBin
            ExeName        = 'e2eSig_TamperedManifest_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 104
            SkipSelfUpdate = $true
        },

        # ── Version parsing / 4-segment revision scenarios ───────────────────
        # Reuse HappyZip (serves 2.0.0) by varying --force-local-version only.

        @{
            # Corrupt local version string must fall back to "outdated" (203)
            # rather than hard-failing with NV_E_PRODUCT_DETECTION (105).
            Name           = 'CorruptLocalVersion'
            SourceBin      = $MainBin
            ExeName        = 'e2e_HappyZip_Updater.exe'
            LocalVersion   = 'not.a.version'
            ExpectedExit   = 203
            SkipSelfUpdate = $true
        },
        @{
            # Local 4-segment "2.0.0.5" > server "2.0.0" (revision 0) => up-to-date.
            Name           = 'LocalRevisionUpToDate'
            SourceBin      = $MainBin
            ExeName        = 'e2e_HappyZip_Updater.exe'
            LocalVersion   = '2.0.0.5'
            ExpectedExit   = 202
            SkipSelfUpdate = $true
        },
        @{
            # Local "2.0.0.0" equals server "2.0.0" (absent revision == 0) => up-to-date.
            Name           = 'LocalRevisionEqual'
            SourceBin      = $MainBin
            ExeName        = 'e2e_HappyZip_Updater.exe'
            LocalVersion   = '2.0.0.0'
            ExpectedExit   = 202
            SkipSelfUpdate = $true
        },
        @{
            # Server advertises 4-segment "2.0.0.1"; local "2.0.0" (revision 0) is older => outdated.
            Name           = 'ServerRevisionOutdated'
            SourceBin      = $MainBin
            ExeName        = 'e2e_FourSegment_Updater.exe'
            LocalVersion   = '2.0.0'
            ExpectedExit   = 203
            SkipSelfUpdate = $true
        },

        # ── Detection-config corruption scenarios (no --force-local-version) ─

        @{
            # Registry value present but unparseable => treated as outdated (203).
            Name            = 'CorruptRegistryVersion'
            SourceBin       = $MainBin
            ExeName         = 'e2e_CorruptRegistry_Updater.exe'
            ExpectedExit    = 203
            SkipSelfUpdate  = $true
            UseLocalVersion = $false
            PreScenario     = {
                Write-Host "  Seeding HKCU:\Software\Nefarius\ViciusE2E with corrupt version..."
                $null = New-Item -Path 'HKCU:\Software\Nefarius\ViciusE2E' -Force
                Set-ItemProperty -Path 'HKCU:\Software\Nefarius\ViciusE2E' `
                    -Name 'Version' -Value 'not_a_version' -Type String
            }
            PostScenario    = {
                Write-Host "  Cleaning up HKCU:\Software\Nefarius\ViciusE2E..."
                Remove-Item -Path 'HKCU:\Software\Nefarius\ViciusE2E' -Recurse -Force `
                    -ErrorAction SilentlyContinue
            }
        },
        @{
            # Non-PE file has no version resource; GetWin32ResourceFileVersion returns 0.0.1,
            # which is older than server 2.0.0 => outdated => 203.
            Name            = 'CorruptFileVersion'
            SourceBin       = $MainBin
            ExeName         = 'e2e_CorruptFileVersion_Updater.exe'
            ExpectedExit    = 203
            SkipSelfUpdate  = $true
            UseLocalVersion = $false
        },
        @{
            # Negative control: registry key absent entirely => NV_E_PRODUCT_DETECTION (105).
            # Confirms missing-key stays a hard error, not a graceful outdated fallback.
            Name            = 'MissingRegistryKey'
            SourceBin       = $MainBin
            ExeName         = 'e2e_MissingRegistry_Updater.exe'
            ExpectedExit    = 105
            SkipSelfUpdate  = $true
            UseLocalVersion = $false
        }
    )

    foreach ($s in $scenarios) {
        $invokeParams = @{
            Name            = $s.Name
            SourceBin       = $s.SourceBin
            ExeName         = $s.ExeName
            LocalVersion    = $s.ContainsKey('LocalVersion')    ? $s.LocalVersion                    : ''
            ExpectedExit    = $s.ExpectedExit
            SkipSelfUpdate  = $s.ContainsKey('SkipSelfUpdate')  ? [bool]$s.SkipSelfUpdate             : $true
            NeedsInstall    = $s.ContainsKey('NeedsInstall')    ? [bool]$s.NeedsInstall               : $false
            UseLocalVersion = $s.ContainsKey('UseLocalVersion') ? [bool]$s.UseLocalVersion            : $true
            Sidecar         = $s.ContainsKey('Sidecar')         ? $s.Sidecar                          : $null
            PreScenario     = $s.ContainsKey('PreScenario')     ? $s.PreScenario                      : $null
            PostScenario    = $s.ContainsKey('PostScenario')    ? $s.PostScenario                     : $null
        }
        $results.Add((Invoke-Scenario @invokeParams))
    }

} finally {
    Stop-E2EServer $serverJob

    # Restore env vars cleared on finish
    Remove-Item Env:\VICIUS_E2E        -ErrorAction SilentlyContinue
    Remove-Item Env:\E2E_ARTIFACTS_DIR -ErrorAction SilentlyContinue
    Remove-Item Env:\ASPNETCORE_URLS   -ErrorAction SilentlyContinue
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

Write-Header 'E2E Test Summary'

$passed = 0
$failed = 0
foreach ($r in $results) {
    $icon   = if ($r.Passed) { '✔' } else { '✘' }
    $detail = if ($r.Passed) { '' } else { " (got $($r.Got), expected $($r.Expected))" }
    Write-Host "  $icon  $($r.Name)$detail"
    if ($r.Passed) { $passed++ } else { $failed++ }
}

Write-Host ''
Write-Host "  Passed: $passed   Failed: $failed"
Write-Host ''

if ($failed -gt 0) {
    Write-Error "E2E suite FAILED: $failed scenario(s) did not match the expected exit code."
    exit 1
}

Write-Host 'All E2E scenarios passed.'
exit 0
