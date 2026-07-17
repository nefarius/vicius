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
        NV_S_INSTANCE_ALREADY_RUNNING = 210
        NV_E_SERVER_RESPONSE = 104
        NV_E_SIGNATURE_INVALID = 116
        NV_E_DOWNLOAD_FAILED = 107

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
    E2E_ARTIFACTS_DIR: directory pre-populated by CI with payload.zip, payload_edge.zip,
    setup.exe, updater_selfupdate.exe, SignedManifest/, and TamperedManifest/ subdirectories.

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

    [string] $LogDir = (Join-Path $PSScriptRoot 'e2e-logs'),

    # Optional: path to the minisign .key file used by the server for dynamic manifest signing.
    # When supplied together with -MinisignPassword the DynamicSignedManifest / DynamicTamperedManifest
    # scenarios are enabled.
    [string] $MinisignSecKey = '',

    # Optional: password for the minisign secret key supplied via -MinisignSecKey.
    [string] $MinisignPassword = ''
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
    if ($MinisignSecKey)    { $env:E2E_MINISIGN_SECKEY   = $MinisignSecKey }
    if ($MinisignPassword)  { $env:E2E_MINISIGN_PASSWORD = $MinisignPassword }

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
        [scriptblock] $PreScenario        = $null,    # runs before the binary is invoked
        [scriptblock] $PostScenario       = $null,   # runs in finally, even on failure
        [string[]]    $ExpectLogContains    = @(),   # each substring must appear in the log file
        [string[]]    $ExpectLogNotContains = @(),   # none of these substrings may appear in the log file
        [string[]]    $ExtraArgs            = @()    # additional CLI args appended verbatim (e.g. --strict-verification)
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
                          LogFailures = @(); Note = "install step failed" }
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
        if ($ExtraArgs.Count -gt 0) { $args += $ExtraArgs }

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

        # ── Log assertions ────────────────────────────────────────────────────
        $logPassed   = $true
        $logFailures = @()
        if ($ExpectLogContains.Count -gt 0 -or $ExpectLogNotContains.Count -gt 0) {
            $logContent = if (Test-Path $logFile) {
                Get-Content $logFile -Raw -ErrorAction SilentlyContinue
            } else { '' }
            foreach ($substr in $ExpectLogContains) {
                if ($logContent -notlike "*$substr*") {
                    $logPassed = $false
                    $logFailures += "Expected in log: '$substr'"
                }
            }
            foreach ($substr in $ExpectLogNotContains) {
                if ($logContent -like "*$substr*") {
                    $logPassed = $false
                    $logFailures += "NOT expected in log: '$substr'"
                }
            }
        }

        $passed = ($got -eq $ExpectedExit) -and $logPassed
        $status = if ($passed) { 'PASS' } else { 'FAIL' }
        Write-Host "  $status  exit=$got  expected=$ExpectedExit"

        if (-not $passed) {
            Write-Host "  Log tail:"
            if (Test-Path $logFile) {
                Get-Content $logFile -Tail 30 | ForEach-Object { Write-Host "    $_" }
            }
            foreach ($msg in $logFailures) {
                Write-Host "  LOG ASSERTION FAILED: $msg"
            }
        }

        return @{ Name = $Name; Passed = $passed; Expected = $ExpectedExit; Got = $got; LogFailures = $logFailures }
    }
    finally {
        Remove-Item -Path $workDir -Recurse -Force -ErrorAction SilentlyContinue
        if ($null -ne $PostScenario) {
            Write-Host "  Running post-scenario hook..."
            & $PostScenario
        }
    }
}

function Invoke-DuplicateInstanceScenario {
    param(
        [string] $SourceBin,
        [string] $ExeName = 'e2e_HappyZip_Updater.exe'
    )

    $Name = 'DuplicateInstance'
    $ExpectedExit = 210  # NV_S_INSTANCE_ALREADY_RUNNING
    Write-Header "Scenario: $Name (expect exit $ExpectedExit)"

    $workDir = Join-Path $env:TEMP "vicius-e2e-$Name-$(New-Guid)"
    New-Item -ItemType Directory -Path $workDir | Out-Null

    $exePath = Join-Path $workDir $ExeName
    Copy-Item -Path $SourceBin -Destination $exePath

    # Fresh logs each run — stale content must not signal readiness or satisfy assertions.
    $ownerLog = Join-Path $LogDir "$Name-owner.log"
    $dupLog   = Join-Path $LogDir "$Name-duplicate.log"
    Remove-Item -Path $ownerLog, $dupLog -Force -ErrorAction SilentlyContinue

    # Hold process keeps the owner in the product-in-use wait after lock acquisition
    # until we release it (after the duplicate has finished).
    $holdImageName = 'vicius-e2e-hold.exe'
    $holdExe = Join-Path $workDir $holdImageName
    Copy-Item -Path (Join-Path $env:SystemRoot 'System32\ping.exe') -Destination $holdExe

    $sidecarPath = Join-Path $workDir ([System.IO.Path]::GetFileNameWithoutExtension($ExeName) + '.json')
    $sidecar = @{
        instance = @{
            authority = 'Local'
        }
        shared = @{
            productBusyDetection = @{
                imageNames          = @($holdImageName)
                pollIntervalSeconds = 5
                maxWaitMinutes      = 5
            }
        }
    } | ConvertTo-Json -Depth 6
    Set-Content -Path $sidecarPath -Value $sidecar -Encoding utf8

    $ownerProc = $null
    $holdProc  = $null

    try {
        Write-Host "  Starting hold process ($holdImageName)..."
        $holdProc = Start-Process `
            -FilePath $holdExe `
            -ArgumentList '-t', '127.0.0.1' `
            -PassThru -WindowStyle Hidden

        $ownerArgs = @(
            '--force-local-version', '0.0.1',
            '--silent-update',
            '--ignore-busy-state',
            '--skip-self-update',
            '--log-to-file', $ownerLog,
            '--log-level', 'debug'
        )

        Write-Host "  Starting owner: $ExeName $ownerArgs"
        $ownerProc = Start-Process `
            -FilePath $exePath `
            -ArgumentList $ownerArgs `
            -PassThru -NoNewWindow

        # Wait until the owner holds the single-instance lock (or exits unexpectedly).
        $deadline = (Get-Date).AddSeconds(30)
        $lockAcquired = $false
        while ((Get-Date) -lt $deadline) {
            if ($ownerProc.HasExited) {
                throw "Owner exited early with code $($ownerProc.ExitCode) before acquiring the lock."
            }
            if (Test-Path $ownerLog) {
                $ownerLogText = Get-Content $ownerLog -Raw -ErrorAction SilentlyContinue
                if ($ownerLogText -like '*Acquired single-instance lock for*') {
                    $lockAcquired = $true
                    break
                }
            }
            Start-Sleep -Milliseconds 100
        }

        if (-not $lockAcquired) {
            throw 'Owner did not acquire the single-instance lock within 30 seconds.'
        }

        # Confirm the owner entered the product-in-use hold (still alive + hold active).
        $holdDeadline = (Get-Date).AddSeconds(60)
        $inUseHold = $false
        while ((Get-Date) -lt $holdDeadline) {
            if ($ownerProc.HasExited) {
                throw "Owner exited with code $($ownerProc.ExitCode) before the product-in-use hold."
            }
            if (Test-Path $ownerLog) {
                $ownerLogText = Get-Content $ownerLog -Raw -ErrorAction SilentlyContinue
                if ($ownerLogText -like '*Product in use*' -or $ownerLogText -like '*matched process by name*') {
                    $inUseHold = $true
                    break
                }
            }
            Start-Sleep -Milliseconds 200
        }
        if (-not $inUseHold) {
            throw 'Owner did not enter the product-in-use hold before duplicate launch.'
        }

        Write-Host "  Owner holds the lock (PID $($ownerProc.Id)); launching duplicate..."
        $dupArgs = @(
            '--force-local-version', '0.0.1',
            '--silent-update',
            '--ignore-busy-state',
            '--skip-self-update',
            '--log-to-file', $dupLog,
            '--log-level', 'debug'
        )

        $dupProc = Start-Process `
            -FilePath $exePath `
            -ArgumentList $dupArgs `
            -Wait -PassThru -NoNewWindow
        $got = $dupProc.ExitCode

        # Owner must still be holding the lock through the entire duplicate run.
        if ($ownerProc.HasExited) {
            throw "Owner exited with code $($ownerProc.ExitCode) before the duplicate finished."
        }

        # Release the hold only after the duplicate has completed.
        if ($null -ne $holdProc -and -not $holdProc.HasExited) {
            Write-Host "  Releasing hold process (PID $($holdProc.Id))..."
            Stop-Process -Id $holdProc.Id -Force -ErrorAction SilentlyContinue
            $holdProc.WaitForExit(5000) | Out-Null
        }

        $logFailures = @()
        $dupLogText = if (Test-Path $dupLog) {
            Get-Content $dupLog -Raw -ErrorAction SilentlyContinue
        } else { '' }

        $expectedLog = 'Another updater instance is already running for'
        if ($dupLogText -notlike "*$expectedLog*") {
            $logFailures += "Expected in duplicate log: '$expectedLog'"
        }

        $passed = ($got -eq $ExpectedExit) -and ($logFailures.Count -eq 0)
        $status = if ($passed) { 'PASS' } else { 'FAIL' }
        Write-Host "  $status  exit=$got  expected=$ExpectedExit"

        if (-not $passed) {
            Write-Host "  Duplicate log tail:"
            if (Test-Path $dupLog) {
                Get-Content $dupLog -Tail 30 | ForEach-Object { Write-Host "    $_" }
            }
            foreach ($msg in $logFailures) {
                Write-Host "  LOG ASSERTION FAILED: $msg"
            }
        }

        return @{ Name = $Name; Passed = $passed; Expected = $ExpectedExit; Got = $got; LogFailures = $logFailures }
    }
    catch {
        Write-Host "  FAIL  $($_.Exception.Message)"
        if (Test-Path $ownerLog) {
            Write-Host "  Owner log tail:"
            Get-Content $ownerLog -Tail 30 | ForEach-Object { Write-Host "    $_" }
        }
        return @{ Name = $Name; Passed = $false; Expected = $ExpectedExit; Got = $null;
                  LogFailures = @($_.Exception.Message) }
    }
    finally {
        if ($null -ne $holdProc -and -not $holdProc.HasExited) {
            Stop-Process -Id $holdProc.Id -Force -ErrorAction SilentlyContinue
            $holdProc.WaitForExit(5000) | Out-Null
        }
        if ($null -ne $ownerProc -and -not $ownerProc.HasExited) {
            Write-Host "  Stopping owner (PID $($ownerProc.Id))..."
            Stop-Process -Id $ownerProc.Id -Force -ErrorAction SilentlyContinue
            $ownerProc.WaitForExit(5000) | Out-Null
        }
        Remove-Item -Path $workDir -Recurse -Force -ErrorAction SilentlyContinue
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
            # Archive-extraction edge cases: empty directory entry, a DeleteIfPresent
            # override for a file absent at the destination, and a normal create entry.
            # Regression coverage for crashes/uncaught exceptions previously thrown out
            # of the setup task by malformed or edge-case ZIP payloads.
            Name           = 'ZipEdgeCases'
            SourceBin      = $MainBin
            ExeName        = 'e2e_ZipEdgeCases_Updater.exe'
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
            # SignedManifest's fixture (tests/e2e's SignedManifest/updates.json) sets
            # shared.signatureVerificationMode = "Disabled" and is served with a valid
            # minisig signature against $SigBin's compiled-in NV_MANIFEST_PUBLIC_KEY, so the
            # override must be applied (not rejected) — proving a verified manifest CAN apply
            # an allowed policy override. Absence of the "Ignoring remote ... override" log
            # lines (added by the verification-policy trust-boundary hardening) is the
            # positive-control signal for this.
            Name              = 'SignedManifest'
            SourceBin         = $SigBin
            ExeName           = 'e2eSig_SignedManifest_Updater.exe'
            LocalVersion      = '0.0.1'
            ExpectedExit      = 203
            SkipSelfUpdate    = $true
            ExpectLogNotContains = @('Ignoring remote signatureVerificationMode override')
        },
        @{
            Name           = 'TamperedManifest'
            SourceBin      = $SigBin
            ExeName        = 'e2eSig_TamperedManifest_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 104
            SkipSelfUpdate = $true
        },
        @{
            # Negative control for SignedManifest: same signed manifest/override, but
            # --strict-verification is passed locally on the CLI. A local operator flag must
            # remain dominant over even a *verified* remote override: the override is
            # rejected (log line present) even though the manifest signature itself is valid,
            # AND --strict-verification additionally escalates the local default policy from
            # WhenPresent/Relaxed to Required/Strict (see InstanceConfig::InstanceConfig).
            # Since the test payload.zip is unsigned, Required mode then hard-fails it with
            # NV_E_SIGNATURE_INVALID (116) instead of the 203 every other happy-path scenario
            # gets — the strongest possible proof that the remote "Disabled" override never
            # took effect. Reuses the SignedManifest fixture/endpoint (same tenant path via
            # ExeName) so the manifest and its signature are identical to that scenario —
            # only the local --strict-verification flag differs.
            Name                 = 'StrictVerificationDominant'
            SourceBin            = $SigBin
            ExeName              = 'e2eSig_SignedManifest_Updater.exe'
            LocalVersion         = '0.0.1'
            ExpectedExit         = 116
            SkipSelfUpdate       = $true
            ExtraArgs            = @('--strict-verification')
            ExpectLogContains    = @('Ignoring remote signatureVerificationMode override')
        },
        @{
            # Unsigned manifest (no NV_MANIFEST_PUBLIC_KEY / .minisig involved, served over
            # $MainBin's plain HTTP loopback) attempts to weaken all four signature-policy
            # fields. The update still succeeds (local default WhenPresent already accepts
            # the unsigned test payload) but every override must be logged as rejected,
            # proving an unsigned manifest cannot disable verification.
            Name              = 'UnsignedOverrideRejected'
            SourceBin         = $MainBin
            ExeName           = 'e2e_UnsignedOverrideRejected_Updater.exe'
            LocalVersion      = '0.0.1'
            ExpectedExit      = 203
            SkipSelfUpdate    = $true
            ExpectLogContains = @(
                'Ignoring remote signatureVerificationMode override',
                'Ignoring remote signaturePolicy override',
                'Ignoring remote signatureStrategy override',
                'Ignoring remote signatureConfig (certificate pin) override'
            )
        },

        # ── Dynamic server-side signing (minisign-net) scenarios ─────────────
        # Reuse $SigBin (same compiled NV_MANIFEST_PUBLIC_KEY); the server signs
        # the manifest at request time via MinisignManifestSigner.
        # These scenarios are skipped when -MinisignSecKey / -MinisignPassword are absent.

        @{
            Name           = 'DynamicSignedManifest'
            SourceBin      = $SigBin
            ExeName        = 'e2eSigDyn_DynamicSignedManifest_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 203
            SkipSelfUpdate = $true
            SkipWhen       = (-not $MinisignSecKey -or -not $MinisignPassword)
        },
        @{
            # Server returns a tampered manifest body but signs the canonical body,
            # so the client's Ed25519 verification fails → NV_E_SERVER_RESPONSE (104).
            Name           = 'DynamicTamperedManifest'
            SourceBin      = $SigBin
            ExeName        = 'e2eSigDyn_DynamicTamperedManifest_Updater.exe'
            LocalVersion   = '0.0.1'
            ExpectedExit   = 104
            SkipSelfUpdate = $true
            SkipWhen       = (-not $MinisignSecKey -or -not $MinisignPassword)
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
            # Non-PE file has no version resource; GetWin32ResourceFileVersion returns
            # std::unexpected, which the caller maps to outdated => 203.
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
        },

        # ── Network-resilience scenarios ─────────────────────────────────────

        @{
            # Primary manifest URL has no matching endpoint (404).
            # Sidecar provides a fallback URL pointing to the working HappyZip endpoint.
            # The updater must detect the HTTP error and retry the fallback => 203.
            Name              = 'ManifestFallback'
            SourceBin         = $MainBin
            ExeName           = 'e2e_ManifestFallback_Updater.exe'
            LocalVersion      = '0.0.1'
            ExpectedExit      = 203
            SkipSelfUpdate    = $true
            Sidecar           = @{
                Name    = 'e2e_ManifestFallback_Updater.json'
                Content = '{"instance":{"fallbackServerUrlTemplates":["http://localhost:5200/api/e2e/HappyZip/updates.json"]}}'
            }
            ExpectLogContains = @('attempting fallback')
        },
        @{
            # Primary manifest URL returns text/html 200 (captive-portal / block-page response).
            # Sidecar provides a fallback URL. The updater must detect the non-JSON body
            # and try the fallback URL => 203.
            Name              = 'BlockPageFallback'
            SourceBin         = $MainBin
            ExeName           = 'e2e_BlockPage_Updater.exe'
            LocalVersion      = '0.0.1'
            ExpectedExit      = 203
            SkipSelfUpdate    = $true
            Sidecar           = @{
                Name    = 'e2e_BlockPage_Updater.json'
                Content = '{"instance":{"fallbackServerUrlTemplates":["http://localhost:5200/api/e2e/HappyZip/updates.json"]}}'
            }
            ExpectLogContains = @('Non-JSON response received, attempting fallback')
        },
        @{
            # Manifest advertises a broken primary download URL (404) plus a working mirror.
            # The updater must exhaust the primary, switch to the mirror, and complete => 203.
            Name              = 'DownloadMirrorFailover'
            SourceBin         = $MainBin
            ExeName           = 'e2e_MirrorFailover_Updater.exe'
            LocalVersion      = '0.0.1'
            ExpectedExit      = 203
            SkipSelfUpdate    = $true
            ExpectLogContains = @('Switching to mirror URL')
        },
        @{
            # Manifest URL uses hostname 'localhostpinned' which does not resolve in system DNS.
            # Sidecar pins localhostpinned:5200 -> 127.0.0.1 via network.pinnedHosts (CURLOPT_RESOLVE).
            # Without the pin this would be a DNS failure (104); success (203) proves the pin fired.
            Name              = 'PinnedHostResolve'
            SourceBin         = $MainBin
            ExeName           = 'e2e_PinnedHostResolve_Updater.exe'
            LocalVersion      = '0.0.1'
            ExpectedExit      = 203
            SkipSelfUpdate    = $true
            Sidecar           = @{
                Name    = 'e2e_PinnedHostResolve_Updater.json'
                Content = '{"instance":{"serverUrlTemplate":"http://localhostpinned:5200/api/e2e/HappyZip/updates.json","network":{"pinnedHosts":[{"host":"localhostpinned","port":5200,"address":"127.0.0.1"}]}}}'
            }
            ExpectLogContains = @('Network config loaded:')
        },
        @{
            # Negative control for PinnedHostResolve: same unresolvable URL but no pin in sidecar.
            # Must fail with NV_E_SERVER_RESPONSE (104) because DNS cannot resolve localhostpinned.
            Name            = 'PinnedHostResolveNegative'
            SourceBin       = $MainBin
            ExeName         = 'e2e_PinnedHostNeg_Updater.exe'
            LocalVersion    = '0.0.1'
            ExpectedExit    = 104
            SkipSelfUpdate  = $true
            Sidecar         = @{
                Name    = 'e2e_PinnedHostNeg_Updater.json'
                Content = '{"instance":{"serverUrlTemplate":"http://localhostpinned:5200/api/e2e/HappyZip/updates.json"}}'
            }
        },
        @{
            # Sidecar sets network.proxyMode = "None" (direct connection, disables any inherited proxy).
            # The update must still succeed against the local test server => 203.
            Name              = 'DirectNoProxy'
            SourceBin         = $MainBin
            ExeName           = 'e2e_NoProxy_Updater.exe'
            LocalVersion      = '0.0.1'
            ExpectedExit      = 203
            SkipSelfUpdate    = $true
            Sidecar           = @{
                Name    = 'e2e_NoProxy_Updater.json'
                Content = '{"instance":{"serverUrlTemplate":"http://localhost:5200/api/e2e/HappyZip/updates.json","network":{"proxyMode":"None"}}}'
            }
            ExpectLogContains = @('Network config loaded:')
        },

        # ── Download/URL boundary hardening scenarios ────────────────────────

        @{
            # The download URL 302-redirects to the real artifact. CURLOPT_REDIR_PROTOCOLS_STR
            # is restricted to https-only (RestrictRedirectProtocols), so libcurl must refuse
            # to follow this plain-HTTP redirect target and the download must fail.
            Name              = 'RedirectDowngradeRejected'
            SourceBin         = $MainBin
            ExeName           = 'e2e_RedirectDowngrade_Updater.exe'
            LocalVersion      = '0.0.1'
            ExpectedExit      = 107  # NV_E_DOWNLOAD_FAILED
            SkipSelfUpdate    = $true
            Sidecar           = @{
                Name    = 'e2e_RedirectDowngrade_Updater.json'
                Content = '{"instance":{"serverUrlTemplate":"http://localhost:5200/api/e2e/RedirectDowngrade/updates.json"}}'
            }
        },
        @{
            # Server sends a quoted, path-traversal Content-Disposition filename
            # ("..\..\evil.exe"). The sanitizer must reject it outright (log warning),
            # keep the original temp file name, and the update must still complete.
            Name              = 'MaliciousContentDispositionRejected'
            SourceBin         = $MainBin
            ExeName           = 'e2e_MaliciousCD_Updater.exe'
            LocalVersion      = '0.0.1'
            ExpectedExit      = 203
            SkipSelfUpdate    = $true
            Sidecar           = @{
                Name    = 'e2e_MaliciousCD_Updater.json'
                Content = '{"instance":{"serverUrlTemplate":"http://localhost:5200/api/e2e/MaliciousContentDisposition/updates.json"}}'
            }
            ExpectLogContains = @('Rejecting Content-Disposition filename')
        },
        @{
            # Regression for exact (not substring) pinned-host matching: the manifest host
            # merely *contains* the pinned host name as a substring
            # ("localhostpinnedx.invalid" starts with the pinned "localhostpinned"). The old
            # requestUrl.find(p.host) check would incorrectly treat this as a pin match and
            # attempt a DoH/pinned-IP recovery retry; the fix requires an exact
            # case-insensitive hostname match, so no recovery is attempted and the DNS
            # failure surfaces immediately without a spurious external DoH lookup.
            # Hostname uses the reserved .invalid TLD so it is guaranteed not to resolve
            # via system DNS (RFC 6761), while still starting with "localhost..." so the
            # debug-build loopback allowance in IsAllowedDownloadUrl lets the request
            # through to the pin-matching code.
            Name                 = 'PinnedHostSubstringNotMatched'
            SourceBin            = $MainBin
            ExeName              = 'e2e_PinnedHostSubstring_Updater.exe'
            LocalVersion         = '0.0.1'
            ExpectedExit         = 104  # NV_E_SERVER_RESPONSE
            SkipSelfUpdate       = $true
            Sidecar              = @{
                Name    = 'e2e_PinnedHostSubstring_Updater.json'
                Content = '{"instance":{"serverUrlTemplate":"http://localhostpinnedx.invalid:5200/api/e2e/HappyZip/updates.json","network":{"proxyMode":"None","pinnedHosts":[{"host":"localhostpinned","port":5200,"address":"127.0.0.1"}]}}}'
            }
            ExpectLogNotContains = @('retrying with DoH/pinned-IP recovery')
        }
    )

    foreach ($s in $scenarios) {
        if ($s.ContainsKey('SkipWhen') -and $s.SkipWhen) {
            Write-Host "  SKIP  $($s.Name) (prerequisites not available)"
            $results.Add(@{ Name = $s.Name; Passed = $true; Expected = $s.ExpectedExit; Got = $null; LogFailures = @(); Skipped = $true })
            continue
        }

        $invokeParams = @{
            Name            = $s.Name
            SourceBin       = $s.SourceBin
            ExeName         = $s.ExeName
            LocalVersion    = $s.ContainsKey('LocalVersion')    ? $s.LocalVersion                    : ''
            ExpectedExit    = $s.ExpectedExit
            SkipSelfUpdate  = $s.ContainsKey('SkipSelfUpdate')  ? [bool]$s.SkipSelfUpdate             : $true
            NeedsInstall    = $s.ContainsKey('NeedsInstall')    ? [bool]$s.NeedsInstall               : $false
            UseLocalVersion = $s.ContainsKey('UseLocalVersion') ? [bool]$s.UseLocalVersion            : $true
            Sidecar              = $s.ContainsKey('Sidecar')              ? $s.Sidecar              : $null
            PreScenario          = $s.ContainsKey('PreScenario')          ? $s.PreScenario          : $null
            PostScenario         = $s.ContainsKey('PostScenario')         ? $s.PostScenario         : $null
            ExpectLogContains    = $s.ContainsKey('ExpectLogContains')    ? $s.ExpectLogContains    : @()
            ExpectLogNotContains = $s.ContainsKey('ExpectLogNotContains') ? $s.ExpectLogNotContains : @()
            ExtraArgs            = $s.ContainsKey('ExtraArgs')            ? $s.ExtraArgs            : @()
        }
        $results.Add((Invoke-Scenario @invokeParams))
    }

    # Concurrency regression: second launch of the same executable must exit with
    # NV_S_INSTANCE_ALREADY_RUNNING (210) after signaling the owner.
    $results.Add((Invoke-DuplicateInstanceScenario -SourceBin $MainBin))

    # ── Win32 process-launch argument round trip ─────────────────────────────
    # The manifest's launchArguments for ProcessArgsRoundTrip is a raw command-line
    # fragment with a quoted space, an embedded escaped quote, and a trailing escaped
    # backslash (see E2EProcessArgsRoundTripEndpoint). ExecuteSetup appends it verbatim
    # after the quoted setup.exe path and launches it via a writable CreateProcessA
    # command-line buffer with an explicit lpApplicationName. The setup stub echoes its
    # received argv (post CreateProcess-splitting) to E2E_ARGS_LOG_FILE, which the
    # updater process propagates to it by simple environment inheritance. Regression
    # coverage for the const_cast<LPSTR> removal / quoting changes in
    # src/InstanceConfig.Setup.cpp: a broken writable buffer or bad quoting would
    # corrupt or drop these tokens even though the exit code alone would still be 0/203.
    $processArgsLogFile = Join-Path $LogDir 'ProcessArgsRoundTrip.args.log'
    Remove-Item -Path $processArgsLogFile -ErrorAction SilentlyContinue

    $processArgsResult = Invoke-Scenario -Name 'ProcessArgsRoundTrip' `
        -SourceBin $MainBin -ExeName 'e2e_ProcessArgsRoundTrip_Updater.exe' `
        -LocalVersion '0.0.1' -ExpectedExit 203 -SkipSelfUpdate $true `
        -PreScenario  { $env:E2E_ARGS_LOG_FILE = $processArgsLogFile } `
        -PostScenario { Remove-Item Env:\E2E_ARGS_LOG_FILE -ErrorAction SilentlyContinue }

    $expectedArgs = @('arg with spaces', 'quote"inside', 'trailingback\')
    $actualArgs   = if (Test-Path $processArgsLogFile) { @(Get-Content $processArgsLogFile) } else { @() }
    if (@(Compare-Object $expectedArgs $actualArgs -SyncWindow 0).Count -ne 0) {
        $processArgsResult.Passed = $false
        $processArgsResult.LogFailures += "argv round-trip mismatch: expected [$($expectedArgs -join '|')], got [$($actualArgs -join '|')]"
        Write-Host "  FAIL  ProcessArgsRoundTrip argv mismatch: expected [$($expectedArgs -join '|')], got [$($actualArgs -join '|')]"
    }
    $results.Add($processArgsResult)

} finally {
    Stop-E2EServer $serverJob

    # Restore env vars cleared on finish
    Remove-Item Env:\VICIUS_E2E              -ErrorAction SilentlyContinue
    Remove-Item Env:\E2E_ARTIFACTS_DIR       -ErrorAction SilentlyContinue
    Remove-Item Env:\ASPNETCORE_URLS         -ErrorAction SilentlyContinue
    Remove-Item Env:\E2E_MINISIGN_SECKEY     -ErrorAction SilentlyContinue
    Remove-Item Env:\E2E_MINISIGN_PASSWORD   -ErrorAction SilentlyContinue
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

Write-Header 'E2E Test Summary'

$passed  = 0
$failed  = 0
$skipped = 0
foreach ($r in $results) {
    if ($r.ContainsKey('Skipped') -and $r.Skipped) {
        Write-Host "  -  $($r.Name) (skipped)"
        $skipped++
        continue
    }
    $icon   = if ($r.Passed) { '✔' } else { '✘' }
    $detail = if ($r.Passed) { '' } else { " (got $($r.Got), expected $($r.Expected))" }
    if (-not $r.Passed -and $r.LogFailures.Count -gt 0) {
        $detail += " [$($r.LogFailures -join '; ')]"
    }
    Write-Host "  $icon  $($r.Name)$detail"
    if ($r.Passed) { $passed++ } else { $failed++ }
}

Write-Host ''
Write-Host "  Passed: $passed   Failed: $failed   Skipped: $skipped"
Write-Host ''

if ($failed -gt 0) {
    Write-Error "E2E suite FAILED: $failed scenario(s) did not match the expected exit code."
    exit 1
}

Write-Host 'All E2E scenarios passed.'
exit 0
