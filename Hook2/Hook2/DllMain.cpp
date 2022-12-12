#include "DllMain.h"

using namespace::std;

void Start()
{

#ifdef DEBUG	

	AllocConsole();
	freopen_s(&pCout, "conout$", "w", stdout);

#endif // DEBUG

	if (FAILED(FindWindowDestopWorker()))
		return;

	DWORD pID = NULL;
	GetWindowThreadProcessId(g_hOwnWnd, &pID);

	if (GetCurrentProcessId() != pID)
		return;

	if (!CheckMutexForDll())
		return;

	DWORD Err = NULL;
	if (g_hOwnWnd && (g_pOrigWndProc == NULL))
	{

#ifdef DEBUG
		cout << "g_hOwnWnd: " << g_hOwnWnd << endl;
#endif // DEBUG

		g_pOrigWndProc = SetWindowLongPtr(g_hOwnWnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);

		Err = GetLastError();
	}
		
#ifdef DEBUG

	if (g_pOrigWndProc)
	{
		cout << "g_pOrigWndProc: " << g_pOrigWndProc << endl;
	}
	else
	{
		cout << "g_pOrigWndProc: " << g_pOrigWndProc << endl;
		cout << "Error: " << Err << endl;
	}

#endif // DEBUG
	
}

BOOL CALLBACK enumWindowsProc(__in  HWND hWnd, __in  LPARAM lParam)
{

	HWND p = FindWindowExW(hWnd, NULL, L"SHELLDLL_DefView", NULL);

	if (p != NULL)
	{
		g_hOwnWnd = FindWindowExW(NULL, hWnd, L"WorkerW", NULL);
	}

	return TRUE;
}

HRESULT FindWindowDestopWorker()
{

	HWND hTempWnd = NULL;
	hTempWnd = FindWindowW(L"Progman", L"Program Manager");
	if (hTempWnd == NULL)
		return S_FALSE;
	if (SendMessageTimeout(hTempWnd, 0x052C, NULL, NULL, SMTO_NORMAL, 1000, NULL) == NULL)
		return S_FALSE;
	EnumWindows((WNDENUMPROC)enumWindowsProc, NULL);

	return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
// 
//	¬ызываетс€ каждый раз, когда приложение получает системное сообщение, смотрим на 
//  сообщение которое пришло, если это сообщение которое нам нужно, обробатываем его, 
//  и передаем сообщение дальше.
// 
////////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	if (message == WM_NCCALCSIZE)
	{
		LPWINDOWPOS wp;
		ZeroMemory(&wp, sizeof(wp));

		LPNCCALCSIZE_PARAMS ncpam;
		ZeroMemory(&ncpam, sizeof(ncpam));

		if (lParam && g_hWndBase)
		{
			ncpam = (NCCALCSIZE_PARAMS*)lParam;

			wp = ncpam->lppos;

			if (wp->cx && wp->cy)
			{
				SendMessageTimeout(g_hWndBase, WM_CLIENTRECTCHANGE, wp->cx, wp->cy, SMTO_NORMAL, 1000, NULL);
			}

		}

	}

	return CallWindowProc((WNDPROC)g_pOrigWndProc, hWnd, message, wParam, lParam);
}

extern "C" __declspec(dllexport)
////////////////////////////////////////////////////////////////////////////////////////
// 
//	ѕолучени€ дескриптора приложени€, дл€ отправки сообщени€ о изменении размерах окна.
// 
////////////////////////////////////////////////////////////////////////////////////////
void set(HWND ownHWND)
{
	if (ownHWND)
	{
		g_hWndBase = ownHWND;
	}

	return;
}

////////////////////////////////////////////////////////////////////////////////////////
// 
//	ќчистка ресурсов.
// 
////////////////////////////////////////////////////////////////////////////////////////
VOID ClearRes()
{
	if(g_pOrigWndProc) SetWindowLongPtr(g_hOwnWnd, GWLP_WNDPROC, g_pOrigWndProc);

#ifdef DEBUG
	if (pCout) fclose(pCout);
	FreeConsole();
#endif // DEBUG

	//FreeLibraryAndExitThread(hMod, 0);
	return;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		hMod = hModule;
		CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(Start), NULL, NULL, NULL);
		break;

	case DLL_PROCESS_DETACH:

		ClearRes();
		break;
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////
// 
//	ѕроверка существовани€ мьютекса, дл€ того чтобы убедитьс€ что приложение запущенно.
// 
////////////////////////////////////////////////////////////////////////////////////////
BOOL CheckMutexForDll()
{
	g_hMutex = OpenMutex(SYNCHRONIZE, FALSE, L"WallpaperAnimationWin10");

	if (g_hMutex == NULL)
	{
#ifdef DEBUG
		cout << "Mutex not found, app is not working..." << endl;
#endif // DEBUG
		return FALSE;
	}

	CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(WaitForAppClossing), NULL, NULL, NULL); // start another thread running the hooking stuff

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////
// 
//	∆дем пока не закроетс€ основное приложение, как только оно закрываетс€ получаем доступ
//  на мьютекс, и освобождаем его, выгружаем библиотеку.
// 
////////////////////////////////////////////////////////////////////////////////////////
VOID WaitForAppClossing()
{
	WaitForSingleObject(g_hMutex, INFINITE);
	ReleaseMutex(g_hMutex);
	FreeLibraryAndExitThread(hMod,0);
	return;
}
