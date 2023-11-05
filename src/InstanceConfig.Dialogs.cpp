#include "pch.h"
#include "InstanceConfig.hpp"


void models::InstanceConfig::TryDisplayUpToDateDialog(const bool force) const
{
    if (!force && IsSilent())
    {
        spdlog::debug("Silent run, suppressing error dialog");
        return;
    }

    int nClickedButton;

    const auto productName = ConvertAnsiToWide(merged.productName);
    const auto windowTitle = ConvertAnsiToWide(merged.windowTitle);

    std::wstringstream sTitle, sHeader, sBody;

    sTitle << windowTitle;
    sHeader << productName << L" is up to date";
    sBody << L"Your installation of " << productName << L" is on the latest version!";

    const HRESULT hr = TaskDialog(
        nullptr,
        appInstance,
        sTitle.str().c_str(),
        sHeader.str().c_str(),
        sBody.str().c_str(),
        TDCBF_CLOSE_BUTTON,
        TD_INFORMATION_ICON,
        &nClickedButton
    );

    if (FAILED(hr))
    {
        spdlog::error("Unexpected dialog result: {}", hr);
    }
}

void models::InstanceConfig::TryDisplayErrorDialog(
    const std::string& header, const std::string& body, const bool force) const
{
    if (!force && IsSilent())
    {
        spdlog::debug("Silent run, suppressing error dialog");
        return;
    }

    // launch error fallback URL, if set
    if (remote.instance.has_value() && remote.instance.value().errorFallbackUrl.has_value())
    {
        spdlog::debug("Error fallback URL ({}) is set, invoking open action",
                      remote.instance.value().errorFallbackUrl.value());

        ShellExecuteA(
            nullptr,
            "open",
            remote.instance.value().errorFallbackUrl.value().c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL
        );
    }

    int nClickedButton;

    const auto windowTitle = ConvertAnsiToWide(merged.windowTitle);
    const auto windowHeader = ConvertAnsiToWide(header);
    const auto windowBody = ConvertAnsiToWide(body);

    const HRESULT hr = TaskDialog(
        nullptr,
        appInstance,
        windowTitle.c_str(),
        windowHeader.c_str(),
        windowBody.c_str(),
        TDCBF_CLOSE_BUTTON,
        TD_ERROR_ICON,
        &nClickedButton
    );

    if (FAILED(hr))
    {
        spdlog::error("Unexpected dialog result: {}", hr);
    }
}

void models::InstanceConfig::TryDisplayUACDialog(bool force) const
{
    if (!force && IsSilent())
    {
        spdlog::debug("Silent run, suppressing error dialog");
        return;
    }

    int nClickedButton;

    const auto productName = ConvertAnsiToWide(merged.productName);
    const auto windowTitle = ConvertAnsiToWide(merged.windowTitle);

    std::wstringstream sTitle, sHeader, sBody;

    sTitle << windowTitle;
    sHeader << L"New version of " << productName << L" Updater is available";
    sBody << L"A newer version of the " << productName << L" Updater is about to get installed. "
        << L"If a UAC confirmation dialog is coming up after closing this message, "
        << L"please consent to it so the update can succeed.";

    const HRESULT hr = TaskDialog(
        nullptr,
        appInstance,
        sTitle.str().c_str(),
        sHeader.str().c_str(),
        sBody.str().c_str(),
        TDCBF_CLOSE_BUTTON,
        TD_INFORMATION_ICON,
        &nClickedButton
    );

    if (FAILED(hr))
    {
        spdlog::error("Unexpected dialog result: {}", hr);
    }
}
