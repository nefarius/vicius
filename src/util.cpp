#include "Updater.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;


void ActivateWindow(HWND hwnd)
{
	//if it's minimized restore it
	if (IsIconic(hwnd))
	{
		SendMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
	}
	//bring the window to the top and activate it
	SetForegroundWindow(hwnd);
	SetActiveWindow(hwnd);
	SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
	//redraw to prevent the window blank.
	RedrawWindow(hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

std::filesystem::path util::GetImageBasePathW()
{
	wchar_t myPath[MAX_PATH + 1] = { 0 };

	GetModuleFileNameW(
		reinterpret_cast<HINSTANCE>(&__ImageBase),
		myPath,
		MAX_PATH + 1
	);

	return std::wstring(myPath);
}

semver::version util::GetVersionFromFile(const std::filesystem::path& filePath)
{
	DWORD verHandle = 0;
	UINT size = 0;
	LPBYTE lpBuffer = nullptr;
	const DWORD verSize = GetFileVersionInfoSizeA(filePath.string().c_str(), &verHandle);
	std::stringstream versionString;

	if (verSize != NULL)
	{
		const auto verData = new char[verSize];

		if (GetFileVersionInfoA(filePath.string().c_str(), verHandle, verSize, verData))
		{
			if (VerQueryValueA(verData, "\\", (VOID FAR * FAR*) & lpBuffer, &size))
			{
				if (size)
				{
					const auto* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
					if (verInfo->dwSignature == 0xfeef04bd)
					{
						versionString
							<< static_cast<ULONG>(HIWORD(verInfo->dwProductVersionMS)) << "."
							<< static_cast<ULONG>(LOWORD(verInfo->dwProductVersionMS)) << "."
							<< static_cast<ULONG>(HIWORD(verInfo->dwProductVersionLS));
					}
				}
			}
		}
		delete[] verData;
	}

	return semver::version{ versionString.str() };
}
