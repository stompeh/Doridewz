#include "resource.h"

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <Sfc.h>

#pragma comment(lib, "sfc")


std::vector<std::wstring> GetDriveLetters()
{
    
    wchar_t driveSlash[] = {':','\\',0};
    std::vector<std::wstring> foundDrives;
    wchar_t diskLetter = 67; // Start with 'C'
    
    for (short i = 1; i != 25; i++)
    {
        std::wstring concatDrivePath;
        concatDrivePath += diskLetter;
        concatDrivePath += driveSlash;
        UINT drivetype = ::GetDriveTypeW(concatDrivePath.c_str());

        if (drivetype != 5 && drivetype != 0)
        {
            if (std::filesystem::exists(concatDrivePath))
            {
                foundDrives.push_back(concatDrivePath);
            }
        }
        diskLetter++;
    }
    return foundDrives;
}

std::map<LPCWSTR, WORD> resNames;

BOOL CALLBACK EnumIconLang(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage, LONG_PTR lParam)
{
    if (lpType == NULL && lpName == NULL)
    {
        return false;
    }

    resNames.emplace(lpName, wLanguage);
    return true;
}

BOOL CALLBACK EnumIconNames(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName, LONG_PTR lParam)
{
    if (lpType == NULL && lpName == NULL)
    {
        return false;
    }

    if (!::EnumResourceLanguagesExW(hModule, lpType, lpName, EnumIconLang, lParam, 0, 0))
    {
        // if error, just continue
    }

    return true;
}

void ReplaceEXEIconResources(std::filesystem::path* pathStart, LPVOID lpMDcanIcon, DWORD dwMDcanIconSize)
{
    wchar_t exeExt[] = { '.','e','x','e',0 };
    for (auto itr_path : std::filesystem::recursive_directory_iterator(*pathStart, std::filesystem::directory_options::skip_permission_denied))
    {
 
        if (itr_path.path().extension() == exeExt)
        {
            const std::wstring currentPath = itr_path.path().wstring();
            //const std::wstring currentPathFilename = itr_path.path().filename().wstring(); // For debugging

            if (::SfcIsFileProtected(NULL, currentPath.c_str()))
            {
                continue;
            }

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
                    case 1812:
                    case 1813:
                    {
                        
                        std::wcout << currentPath << "\n" << "No icon resources detected, attempting to add\n";
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
                            std::wcout << ::GetLastError() << " Could not update group resource.\n";
                            resNames.clear();
                            continue;
                        }
                        if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_ICON), NULL, NULL, lpMDcanIcon, dwMDcanIconSize))
                        {
                            std::wcout << ::GetLastError() << " Could not update icon resource.\n";
                            resNames.clear();
                            continue;
                        }
                        if (!::EndUpdateResourceW(hUpdateResource, false))
                        {
                            std::wcout << ::GetLastError() << " Could not end update resource.\n";
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

    for (auto &drive : drives)
    {
        std::filesystem::path drivepath = drive.c_str();
        ReplaceEXEIconResources(&drivepath, lpMDcanIcon, dwMDcanIconSize);
    }

    UnlockResource(lpMDcanIcon);
    ::FreeResource(hgMDcanIcon);
    std::wcout << std::endl;

    return 0;
}