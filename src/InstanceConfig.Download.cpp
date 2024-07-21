#include "pch.h"
#include "InstanceConfig.hpp"


bool models::InstanceConfig::DownloadReleaseAsync(int releaseIndex, curl_progress_callback progressFn)
{
    // fail if already in-progress
    if (downloadTask.has_value())
    {
        return false;
    }

    downloadTask = std::async(std::launch::async, &InstanceConfig::DownloadRelease, this, progressFn, releaseIndex);

    return true;
}

[[nodiscard]] bool models::InstanceConfig::GetReleaseDownloadStatus(bool& isDownloading, bool& hasFinished, int& statusCode) const
{
    if (!downloadTask.has_value())
    {
        return false;
    }

    const std::future_status status = (*downloadTask).wait_for(std::chrono::milliseconds(1));

    isDownloading = status == std::future_status::timeout;
    hasFinished = status == std::future_status::ready;

    if (hasFinished)
    {
        statusCode = (*downloadTask).get();
    }

    return true;
}

void models::InstanceConfig::ResetReleaseDownloadState() { downloadTask.reset(); }
