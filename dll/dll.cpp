// dll.cpp : Defines the exported functions for the DLL.
//

#include "framework.h"
#include "dll.h"


EXTERN_C DLL_API void CALLBACK PerformUpdate(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
	MessageBoxA(hwnd, lpszCmdLine, "Hi", MB_OK);
	//MoveFileA("nefarius_HidHide_Updater.exe", "nefarius_HidHide_Updater_2.exe");
	DeleteFileA("nefarius_HidHide_Updater.exe");
	MoveFileA("Updater.exe", "nefarius_HidHide_Updater.exe");
	//DeleteFileA("nefarius_HidHide_Updater_2.exe");
}
