#include "Updater.h"
#include "InstanceConfig.hpp"


void models::InstanceConfig::DisplayUpTpDateDialog() const
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
