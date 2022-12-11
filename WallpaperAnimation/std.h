#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <atlcomcli.h>

#include <d3dcompiler.h>	// Добавились новые заголовки
#include <DirectXMath.h>
#include <string>
#include <strsafe.h>


#include <cstddef>
#include <iterator>
#include <memory>
#include <new>
#include <utility>
#include <vector>
#include <iostream>
#include <io.h>
#include <tchar.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")


bool FileExist(char*);
bool Inject(DWORD, char*);