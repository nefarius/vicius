#!/usr/bin/env pwsh
#Requires -Version 7.0
<#
.SYNOPSIS
    Async-shutdown lifecycle E2E tests for the interactive updater window.

.DESCRIPTION
    run-e2e.ps1 only exercises the headless --silent-update path, which never touches
    the WM_QUIT close handling, the changelog image-download task, or the setup
    stop_source wiring in main.cpp. This script instead launches the interactive
    updater, drives it to the wizard's "download and install" step via the E2E-only
    NV_E2E_AUTO_ADVANCE env var (main.cpp), polls until a background task (changelog
    image fetch, release download, or child setup process) is genuinely in flight
    against an artificially delayed server response, then posts WM_CLOSE to the main
    window -- exactly what a user clicking the titlebar X does -- and asserts the
    process exits within a bounded time with an explicitly allowlisted NV_*/clean
    shutdown exit code.

    Without the async-lifecycle fix, closing while a task is in flight risks either an
    indefinite hang (dropping a not-yet-ready std::shared_future from std::async blocks
    the destructor until the task completes) or a use-after-free (the changelog's
    image-download task keeps calling into curl/D3D after curlpp::terminate() and
    CleanupDeviceD3D() have already run). Bounded, crash-free exit with an expected
    code is the pass criterion.

.PARAMETER MainBin
    Path to the main E2E updater binary (e2e_Main_Updater.exe), built with
    NV_FLAGS_ALLOW_HTTP_DOWNLOAD (required for both the localhost server URL and the
    NV_E2E_AUTO_ADVANCE wizard hook, which only compiles into that build).

.PARAMETER ArtifactsDir
    E2E_ARTIFACTS_DIR: directory containing payload.zip and setup.exe (same fixtures
    used by run-e2e.ps1).

.PARAMETER ServerDir
    Path to the examples/server project directory (for locating the pre-built server.dll).

.PARAMETER LogDir
    Directory where per-scenario log files are written.
#>
param(
    [Parameter(Mandatory)]
    [string] $MainBin,

    [Parameter(Mandatory)]
    [string] $ArtifactsDir,

    [Parameter(Mandatory)]
    [string] $ServerDir,

    [string] $LogDir = (Join-Path $PSScriptRoot 'e2e-lifecycle-logs')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (Test-Path $LogDir) { Remove-Item $LogDir -Recurse -Force }
New-Item -ItemType Directory -Path $LogDir | Out-Null

# ---------------------------------------------------------------------------
# Win32 helpers: find the updater's top-level window by owning PID and post
# WM_CLOSE to it, the same message DefWindowProc sends when the titlebar X /
# Alt+F4 is used.
# ---------------------------------------------------------------------------

Add-Type -Namespace ViciusE2E -Name Win32 -MemberDefinition @'
    [System.Runtime.InteropServices.DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
    public static extern bool PostMessageW(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    public const uint WM_CLOSE = 0x0010;
'@

function Find-MainWindow {
    param([int] $ProcessId, [int] $TimeoutMs = 15000)

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        $found = [IntPtr]::Zero
        $callback = [ViciusE2E.Win32+EnumWindowsProc] {
            param([IntPtr]$hWnd, [IntPtr]$lParam)
            [uint32]$ownerPid = 0
            [void][ViciusE2E.Win32]::GetWindowThreadProcessId($hWnd, [ref]$ownerPid)
            if ($ownerPid -eq $ProcessId -and [ViciusE2E.Win32]::IsWindowVisible($hWnd)) {
                $script:found = $hWnd
                return $false
            }
            return $true
        }
        $script:found = [IntPtr]::Zero
        [void][ViciusE2E.Win32]::EnumWindows($callback, [IntPtr]::Zero)
        if ($script:found -ne [IntPtr]::Zero) { return $script:found }
        Start-Sleep -Milliseconds 100
    }
    return [IntPtr]::Zero
}

function Send-WmClose {
    param([IntPtr] $HWnd)
    [void][ViciusE2E.Win32]::PostMessageW($HWnd, [ViciusE2E.Win32]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
}

# ---------------------------------------------------------------------------
# Server lifecycle (mirrors run-e2e.ps1)
# ---------------------------------------------------------------------------

function Start-E2EServer {
    Write-Host 'Building example server...'
    & dotnet build $ServerDir -c Release --nologo -v quiet | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "dotnet build failed with exit $LASTEXITCODE" }

    Write-Host 'Starting example server...'
    $env:VICIUS_E2E        = '1'
    $env:E2E_ARTIFACTS_DIR = $ArtifactsDir
    $env:ASPNETCORE_URLS   = 'http://localhost:5200'
    $env:ASPNETCORE_ENVIRONMENT = 'Development'

    $serverDll = Join-Path $ServerDir 'bin' 'Release' 'net10.0' 'server.dll'

    $job = Start-Process `
        -FilePath 'dotnet' `
        -ArgumentList $serverDll `
        -PassThru `
        -NoNewWindow `
        -RedirectStandardOutput (Join-Path $LogDir 'server.stdout.txt') `
        -RedirectStandardError  (Join-Path $LogDir 'server.stderr.txt')

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

# ---------------------------------------------------------------------------
# Readiness polling: wait for explicit evidence that the targeted background
# task has started (log marker and/or child process), rather than a fixed sleep.
# ---------------------------------------------------------------------------

function Wait-LifecycleReady {
    param(
        [System.Diagnostics.Process] $Process,
        [string] $LogFile,
        [string] $ReadyLogContains = '',
        [switch] $ReadyAnyChildProcess,  # setup is launched from a GetTempFileName path, not setup.exe
        [int]    $TimeoutMs = 15000
    )

    if (-not $ReadyLogContains -and -not $ReadyAnyChildProcess) {
        throw 'Wait-LifecycleReady requires ReadyLogContains and/or ReadyAnyChildProcess'
    }

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        if ($Process.HasExited) {
            return @{ Ready = $false; Reason = "Process exited before readiness (exit code $($Process.ExitCode))" }
        }

        if ($ReadyLogContains -and (Test-Path -LiteralPath $LogFile)) {
            if (Select-String -LiteralPath $LogFile -Pattern $ReadyLogContains -SimpleMatch -Quiet) {
                return @{ Ready = $true; Reason = "log marker '$ReadyLogContains'" }
            }
        }

        if ($ReadyAnyChildProcess) {
            $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($Process.Id)" -ErrorAction SilentlyContinue)
            if ($children.Count -gt 0) {
                return @{ Ready = $true; Reason = "child process '$($children[0].Name)' (PID $($children[0].ProcessId))" }
            }
        }

        Start-Sleep -Milliseconds 100
    }

    $wanted = @(
        $(if ($ReadyLogContains) { "log marker '$ReadyLogContains'" }),
        $(if ($ReadyAnyChildProcess) { 'any child process' })
    ) | Where-Object { $_ }
    return @{ Ready = $false; Reason = "Timed out after ${TimeoutMs}ms waiting for $($wanted -join ' or ')" }
}

# ---------------------------------------------------------------------------
# Scenario runner
# ---------------------------------------------------------------------------

$results = [System.Collections.Generic.List[hashtable]]::new()

function Invoke-LifecycleScenario {
    param(
        [string]   $Name,
        [string]   $ExeName,
        [hashtable]$Sidecar,
        [int[]]    $ExpectedExitCodes,      # allowlisted NV_*/clean shutdown codes for this scenario
        [string]   $ReadyLogContains = '',  # log substring proving the target task started
        [switch]   $ReadyAnyChildProcess,   # any child of the updater (setup is a temp-named PE)
        [int]      $ReadyTimeoutMs = 15000, # fail if readiness evidence never appears
        [int]      $BoundedExitTimeoutMs,   # process must exit within this long after WM_CLOSE
        [hashtable]$ExtraEnv = @{}
    )

    Write-Host "`n──────────────────────────────────────────────────────────"
    Write-Host "  Scenario: $Name"
    Write-Host "──────────────────────────────────────────────────────────"

    $workDir = Join-Path $env:TEMP "vicius-e2e-lifecycle-$Name-$(New-Guid)"
    New-Item -ItemType Directory -Path $workDir | Out-Null

    $exePath = Join-Path $workDir $ExeName
    Copy-Item -Path $MainBin -Destination $exePath

    $sidecarPath = Join-Path $workDir $Sidecar.Name
    $Sidecar.Content | Set-Content -Path $sidecarPath -Encoding utf8

    $logFile = Join-Path $LogDir "$Name.log"

    $prevEnv = @{}
    foreach ($k in $ExtraEnv.Keys) {
        $prevEnv[$k] = [System.Environment]::GetEnvironmentVariable($k)
        [System.Environment]::SetEnvironmentVariable($k, $ExtraEnv[$k])
    }
    [System.Environment]::SetEnvironmentVariable('NV_E2E_AUTO_ADVANCE', '1')

    $result = @{ Name = $Name; Passed = $false; Note = '' }

    try {
        $proc = Start-Process `
            -FilePath $exePath `
            -ArgumentList '--force-local-version', '0.0.1', '--ignore-busy-state',
                          '--log-to-file', $logFile, '--log-level', 'debug' `
            -PassThru -NoNewWindow

        try {
            Write-Host "  Launched PID $($proc.Id); waiting for main window..."
            $hwnd = Find-MainWindow -ProcessId $proc.Id -TimeoutMs 15000
            if ($hwnd -eq [IntPtr]::Zero) {
                $result.Note = 'Main window never appeared'
                return $result
            }

            Write-Host "  Window found; polling for background-task readiness..."
            $readyParams = @{
                Process             = $proc
                LogFile             = $logFile
                ReadyLogContains    = $ReadyLogContains
                TimeoutMs           = $ReadyTimeoutMs
            }
            if ($ReadyAnyChildProcess) { $readyParams.ReadyAnyChildProcess = $true }
            $ready = Wait-LifecycleReady @readyParams

            if (-not $ready.Ready) {
                $result.Note = $ready.Reason
                return $result
            }

            Write-Host "  Ready ($($ready.Reason)); sending WM_CLOSE..."

            if ($proc.HasExited) {
                $result.Note = "Process already exited before WM_CLOSE was sent (exit code $($proc.ExitCode))"
                return $result
            }

            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            Send-WmClose -HWnd $hwnd

            $exited = $proc.WaitForExit($BoundedExitTimeoutMs)
            $sw.Stop()

            if (-not $exited) {
                $result.Note = "Did not exit within ${BoundedExitTimeoutMs}ms of WM_CLOSE (hang) - killing"
                Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
                return $result
            }

            $elapsedMs = $sw.ElapsedMilliseconds
            $exitCode = $proc.ExitCode

            Write-Host "  Exited $elapsedMs`ms after WM_CLOSE with code $exitCode"

            # A crash surfaces as an NTSTATUS-style exit code (e.g. access violation is
            # 0xC0000005 == -1073741819) rather than one of the small, well-known NV_* codes.
            if ($exitCode -lt 0 -or $exitCode -gt 255) {
                $result.Note = "Crash-shaped exit code $exitCode (0x$('{0:X8}' -f $exitCode))"
                return $result
            }

            # Ordinary non-crash codes that are not on this scenario's allowlist are rejects
            # (e.g. NV_S_UPDATE_FINISHED=203 would mean we closed after the work finished).
            if ($ExpectedExitCodes -notcontains $exitCode) {
                $allowed = ($ExpectedExitCodes | ForEach-Object { $_ }) -join ', '
                $result.Note = "Unexpected exit code $exitCode (allowed: $allowed)"
                return $result
            }

            $result.Passed = $true
            $result.Note = "Clean exit, code $exitCode, ${elapsedMs}ms after WM_CLOSE"
            return $result
        }
        finally {
            if (-not $proc.HasExited) {
                Write-Warning "  Scenario '$Name' left the process running; force-killing."
                Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            }
        }
    }
    finally {
        foreach ($k in $prevEnv.Keys) {
            [System.Environment]::SetEnvironmentVariable($k, $prevEnv[$k])
        }
        [System.Environment]::SetEnvironmentVariable('NV_E2E_AUTO_ADVANCE', $null)
        Remove-Item -Path $workDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

$serverJob = Start-E2EServer

try {
    # Server delays both the changelog image and the release artifact by 8s
    # (E2ELifecycleEndpoint.cs). Poll until the updater log shows the release
    # download has started — at that point the delayed HTTP transfer (and the
    # in-flight image fetch started from the same summary) are genuinely running.
    # Closing then must exit well under the remaining delay via cooperative abort.
    # Expected exit: ERROR_SUCCESS (0) — WM_CLOSE -> WM_DESTROY -> PostQuitMessage(0)
    # leaves main.cpp's status at its ERROR_SUCCESS default when no wizard status
    # was posted first.
    $results.Add((Invoke-LifecycleScenario `
        -Name 'CloseDuringDownloadAndImageFetch' `
        -ExeName 'e2e_LifecycleDownload_Updater.exe' `
        -Sidecar @{
            Name    = 'e2e_LifecycleDownload_Updater.json'
            Content = '{"instance":{"serverUrlTemplate":"http://localhost:5200/api/e2e/Lifecycle/updates.json"}}'
        } `
        -ExpectedExitCodes @(0) `
        -ReadyLogContains 'Starting release download from' `
        -ReadyTimeoutMs 15000 `
        -BoundedExitTimeoutMs 7000))

    # setup.exe sleeps for E2E_EXIT_DELAY_MS (inherited) before exiting. Poll until
    # the child setup process is actually running (or the launch log marker appears),
    # then close — the cooperative stop_source must cut the wait short well before
    # the 8s sleep elapses. Expected exit: ERROR_SUCCESS (0) for the same
    # PostQuitMessage path; NV_S_CLOSED_WHILE_UPDATER_RUNNING (208) is also allowed
    # if a frame races and posts the cancelled-setup status before quit.
    $results.Add((Invoke-LifecycleScenario `
        -Name 'CloseDuringSetup' `
        -ExeName 'e2e_LifecycleSetup_Updater.exe' `
        -Sidecar @{
            Name    = 'e2e_LifecycleSetup_Updater.json'
            Content = '{"instance":{"serverUrlTemplate":"http://localhost:5200/api/e2e/LifecycleSetup/updates.json"}}'
        } `
        -ExpectedExitCodes @(0, 208) `
        -ReadyLogContains 'Setup process launched successfully' `
        -ReadyAnyChildProcess `
        -ReadyTimeoutMs 20000 `
        -BoundedExitTimeoutMs 5000 `
        -ExtraEnv @{ E2E_EXIT_DELAY_MS = '8000' }))
}
finally {
    Stop-E2EServer $serverJob
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

Write-Host "`n──────────────────────────────────────────────────────────"
Write-Host '  Lifecycle E2E summary'
Write-Host '──────────────────────────────────────────────────────────'

$failed = 0
foreach ($r in $results) {
    if ($r.Passed) {
        $mark = 'PASS'
    } else {
        $mark = 'FAIL'
        $failed++
    }
    Write-Host "  [$mark] $($r.Name): $($r.Note)"
}

if ($failed -gt 0) {
    Write-Host "`n$failed scenario(s) failed." -ForegroundColor Red
    exit 1
}

Write-Host "`nAll lifecycle scenarios passed." -ForegroundColor Green
exit 0
