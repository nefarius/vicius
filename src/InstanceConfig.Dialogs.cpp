#include "pch.h"
#include "Common.h"
#include "InstanceConfig.hpp"


void models::InstanceConfig::DisplayUpToDateDialog() const
{
	int nClickedButton;

	const auto productName = ConvertAnsiToWide(shared.productName);
	const auto windowTitle = ConvertAnsiToWide(shared.windowTitle);

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

void models::InstanceConfig::DisplayErrorDialog(const std::string& header, const std::string& body,
                                                const bool force) const
{
	if (!force && IsSilent())
	{
		spdlog::debug("Silent run, suppressing error dialog");
		return;
	}

	int nClickedButton;

	const auto windowTitle = ConvertAnsiToWide(shared.windowTitle);
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
