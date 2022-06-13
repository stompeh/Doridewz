#include "Friends.h"

#include <iostream>
#include <intrin.h>
#include <memory>


typedef HMODULE(WINAPI* d_LoadLibrary)(LPCWSTR lpFileName);
d_LoadLibrary pd_LoadLibraryW = NULL;

HMODULE WINAPI d_GetModuleHandle(LPCWSTR sModuleName)
{
	DWORD64 woww = __readgsqword(0x60);

	//std::unique_ptr<PEB> pPEB = std::make_unique<PEB>(woww);

	//auto ppPEB = std::unique_ptr((PEB*)woww);

	//PEB ProcEnvBlock = *pPEB;

	PEB* __ptr64 pPEB = (PEB * __ptr64)woww;
	unsigned long long wtfbro = __popcnt64(woww);



	return NULL;
}

FARPROC WINAPI d_GetProcAddress(HMODULE Mod, wchar_t * sProcName)
{
	return NULL;
}