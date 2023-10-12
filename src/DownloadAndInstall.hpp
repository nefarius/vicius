#pragma once

enum class DownloadAndInstallStep
{
	Begin,
	Downloading,
	DownloadFailed,
	DownloadSucceeded,
	PrepareInstall,
	InstallLaunchFailed,
	InstallRunning,
	InstallFailed,
	InstallSucceeded
};
