#include "std.h"

bool FileExist(char* name)
{
	return _access(name, 0) != -1;
}

bool Inject(DWORD pID, char* path)
{
	HANDLE proc_handle;
	LPVOID RemoteString;
	LPVOID LoadLibAddy;
	if (pID == 0)
		return false;
	proc_handle = OpenProcess(PROCESS_ALL_ACCESS, false, pID);
	if (proc_handle == 0)
		return false;
	LoadLibAddy = (LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA");
	RemoteString = VirtualAllocEx(proc_handle, NULL, strlen(path), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(proc_handle, RemoteString, path, strlen(path), NULL);

	if (CreateRemoteThread(proc_handle, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibAddy, RemoteString, NULL, NULL) == NULL)
	{
		CloseHandle(proc_handle);
		return false;
	}

	CloseHandle(proc_handle);
	return true;
}