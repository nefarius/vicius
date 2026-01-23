#include "pch.h"
#include "InstanceConfig.hpp"


bool models::InstanceConfig::DownloadReleaseAsync(int releaseIndex, curl_progress_callback progressFn)
{
    // fail if already in-progress
    if (downloadTask.has_value())
    {
        return false;
    }

    abortDownloadRequested.store(false, std::memory_order_relaxed);
    downloadTask = std::async(std::launch::async, &InstanceConfig::DownloadRelease, this, progressFn, releaseIndex);

    return true;
}

[[nodiscard]] bool models::InstanceConfig::GetReleaseDownloadStatus(bool& isDownloading, bool& hasFinished, int& statusCode) const
{
    if (!downloadTask.has_value())
    {
        return false;
    }

    const std::future_status status = downloadTask->wait_for(std::chrono::milliseconds(1));

    isDownloading = status == std::future_status::timeout;
    hasFinished = status == std::future_status::ready;

    if (hasFinished)
    {
        statusCode = downloadTask->get();
    }

    return true;
}

void models::InstanceConfig::ResetReleaseDownloadState() { downloadTask.reset(); }

void models::InstanceConfig::RequestAbortDownload()
{
    abortDownloadRequested.store(true, std::memory_order_relaxed);
}

void models::InstanceConfig::WaitForDownloadToFinish()
{
    if (!downloadTask.has_value())
    {
        return;
    }

    // Wait for completion; DownloadRelease is responsible for observing abortDownloadRequested.
    downloadTask->wait();
}
