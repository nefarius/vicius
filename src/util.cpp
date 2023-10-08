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
