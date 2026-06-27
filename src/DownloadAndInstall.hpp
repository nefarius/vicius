#pragma once

enum class DownloadAndInstallStep
{
    Begin,
    Downloading,
    DownloadFailed,
    DownloadSucceeded,
    Verifying,
    VerificationFailed,
    PrepareInstall,
    InstallRunning,
    InstallFailed,
    InstallSucceeded
};
