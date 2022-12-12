#pragma once
#include "DllMain.h"
#include <Windows.h>
#include <iostream>

#pragma data_seg("SHARED")

HWND		g_hWndBase =	 NULL;
HWND		g_hOwnWnd =		 NULL;
LONG_PTR	g_pOrigWndProc = NULL;
FILE*		pCout =			 NULL;
HMODULE		hMod =			 NULL;
HANDLE		g_hMutex =		 NULL;
HANDLE      g_hMutexExp =    NULL;

#pragma data_seg()

#pragma comment(linker, "/section:SHARED,RWS")  

#define WM_CLIENTRECTCHANGE 0x8001

extern "C" __declspec(dllexport) void set(HWND);

//--------------------------------------------------------------------------------------
// Предварительные объявления функций
//--------------------------------------------------------------------------------------

LRESULT CALLBACK WndProcHook(HWND , UINT , WPARAM , LPARAM );
BOOL CALLBACK enumWindowsProc(__in  HWND hWnd, __in  LPARAM lParam);
HRESULT FindWindowDestopWorker();
VOID ClearRes();
BOOL CheckMutexForDll();
VOID WaitForAppClossing();


