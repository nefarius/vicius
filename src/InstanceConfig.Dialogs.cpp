#include "pch.h"
#include <commctrl.h>
#include <sstream>
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

    const HRESULT hr = TaskDialog(nullptr,
                                  appInstance,
                                  sTitle.str().c_str(),
                                  sHeader.str().c_str(),
                                  sBody.str().c_str(),
                                  TDCBF_CLOSE_BUTTON,
                                  TD_INFORMATION_ICON,
                                  &nClickedButton);

    if (FAILED(hr))
    {
        spdlog::error("Unexpected dialog result: {}", hr);
    }
}

void models::InstanceConfig::TryDisplayErrorDialog(const std::string& header, const std::string& body, const bool force) const
{
    if (!force && IsSilent())
    {
        spdlog::debug("Silent run, suppressing error dialog");
        return;
    }

    // launch error fallback URL, if set
    if (remote.instance.has_value() && remote.instance.value().errorFallbackUrl.has_value())
    {
        spdlog::debug("Error fallback URL ({}) is set, invoking open action", remote.instance.value().errorFallbackUrl.value());

        ShellExecuteA(nullptr, "open", remote.instance.value().errorFallbackUrl.value().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    int nClickedButton;

    const auto windowTitle = ConvertAnsiToWide(merged.windowTitle);
    const auto windowHeader = ConvertAnsiToWide(header);
    const auto windowBody = ConvertAnsiToWide(body);

    const HRESULT hr = TaskDialog(nullptr,
                                  appInstance,
                                  windowTitle.c_str(),
                                  windowHeader.c_str(),
                                  windowBody.c_str(),
                                  TDCBF_CLOSE_BUTTON,
                                  TD_ERROR_ICON,
                                  &nClickedButton);

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

    const HRESULT hr = TaskDialog(nullptr,
                                  appInstance,
                                  sTitle.str().c_str(),
                                  sHeader.str().c_str(),
                                  sBody.str().c_str(),
                                  TDCBF_CLOSE_BUTTON,
                                  TD_INFORMATION_ICON,
                                  &nClickedButton);

    if (FAILED(hr))
    {
        spdlog::error("Unexpected dialog result: {}", hr);
    }
}

std::expected<void, std::string> models::InstanceConfig::ShowUserOptionsDialog()
{
    const auto disabled = AreUpdatesDisabled();
    if (!disabled)
    {
        return std::unexpected(disabled.error());
    }

    const std::wstring windowTitle = ConvertAnsiToWide(merged.windowTitle);
    const std::wstring productName = ConvertAnsiToWide(merged.productName);

    std::wstringstream header;
    std::wstringstream body;

    TASKDIALOGCONFIG tdc{};
    tdc.cbSize = sizeof(tdc);
    tdc.hInstance = appInstance;
    tdc.pszWindowTitle = windowTitle.c_str();
    tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_POSITION_RELATIVE_TO_WINDOW;

    constexpr int kEnableButtonId = 100;
    TASKDIALOG_BUTTON customButtons[2]{};

    if (*disabled)
    {
        header << L"Update notifications are disabled";
        body << productName
             << L" will not check for or offer updates until you enable notifications again.\n\n"
             << L"You can turn them back on now, or later by launching this updater with --show-options.";

        customButtons[0] = {kEnableButtonId, L"Enable update notifications"};
        customButtons[1] = {IDCANCEL, L"Keep disabled"};
        tdc.pButtons = customButtons;
        tdc.cButtons = 2;
        tdc.nDefaultButton = IDCANCEL;
        tdc.pszMainIcon = TD_WARNING_ICON;
    }
    else
    {
        header << L"Update notifications are enabled";
        body << productName
             << L" will offer updates when a newer version is available.\n\n"
             << L"If you later disable notifications, launch this updater with --show-options to turn them back on.";

        tdc.dwCommonButtons = TDCBF_CLOSE_BUTTON;
        tdc.pszMainIcon = TD_INFORMATION_ICON;
    }

    const std::wstring headerText = header.str();
    const std::wstring bodyText = body.str();
    tdc.pszMainInstruction = headerText.c_str();
    tdc.pszContent = bodyText.c_str();

    int clicked = 0;
    const HRESULT hrDialog = TaskDialogIndirect(&tdc, &clicked, nullptr, nullptr);
    if (FAILED(hrDialog))
    {
        spdlog::error("ShowUserOptionsDialog: TaskDialogIndirect failed: {:#x}", static_cast<unsigned>(hrDialog));
        return std::unexpected(
            std::format("Failed to display options dialog (HRESULT {:#x})", static_cast<unsigned>(hrDialog)));
    }

    if (*disabled && clicked == kEnableButtonId)
    {
        if (const auto r = SetUpdatesDisabled(false); !r)
        {
            return std::unexpected(r.error());
        }
    }

    return {};
}
