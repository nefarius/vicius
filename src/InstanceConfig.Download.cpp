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

[[nodiscard]] std::optional<models::InstanceConfig::DownloadStatus> models::InstanceConfig::GetReleaseDownloadStatus() const
{
    if (!downloadTask.has_value())
    {
        return std::nullopt;
    }

    const std::future_status futureStatus = downloadTask->wait_for(std::chrono::milliseconds(1));

    DownloadStatus status{};
    status.isDownloading = futureStatus == std::future_status::timeout;
    status.hasFinished = futureStatus == std::future_status::ready;

    if (status.hasFinished)
    {
        // DownloadRelease reports failures via std::expected; guard against any exception
        // that escaped it anyway so a polling call on the render thread can never propagate
        // one out (that would unwind through the ImGui/DirectX frame and crash the app).
        try
        {
            status.result = downloadTask->get();
        }
        catch (const std::exception& e)
        {
            status.result = std::unexpected(std::format("Download task failed with an exception: {}", e.what()));
        }
        catch (...)
        {
            status.result = std::unexpected("Download task failed with an unknown exception");
        }
    }

    return status;
}

void models::InstanceConfig::ResetReleaseDownloadState() { downloadTask.reset(); }

void models::InstanceConfig::RequestAbortDownload()
{
    const bool was = abortDownloadRequested.exchange(true, std::memory_order_relaxed);
    if (!was)
    {
        spdlog::info("Download abort requested");
    }
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
