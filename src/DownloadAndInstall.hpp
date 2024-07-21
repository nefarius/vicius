#pragma once

enum class DownloadAndInstallStep
{
    Begin,
    Downloading,
    DownloadFailed,
    DownloadSucceeded,
    PrepareInstall,
    InstallRunning,
    InstallFailed,
    InstallSucceeded
};
