#pragma once

#include <string>
#include <vector>
#include <Windows.h>
#include "LogicalDrive.h"

class LogicalDrivesEnumerator {
public:
	std::vector<LOGICAL_DRIVE> LogicalDrives;

	LogicalDrivesEnumerator();

private:
	DWORD drivesBuffLen = MAX_PATH;
	WCHAR drivesBuff[MAX_PATH] = { 0 };
	DWORD bufferLength = 0;
	std::vector<std::wstring> DriveNames = {};

	void ParseDriveBuffer();
	void EnumerateDriveTypes();
};

