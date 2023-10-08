#include "Updater.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;


std::filesystem::path GetImageBasePathW()
{
	wchar_t myPath[MAX_PATH + 1] = { 0 };

	GetModuleFileNameW(
		reinterpret_cast<HINSTANCE>(&__ImageBase),
		myPath,
		MAX_PATH + 1
	);

	return std::wstring(myPath);
}

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
	RedrawWindow(hwnd, NULL, 0, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}
