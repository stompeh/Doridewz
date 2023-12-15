#include "LogicalDrivesEnumerator.h"

// public ===========================================

LogicalDrivesEnumerator::LogicalDrivesEnumerator() {

	bufferLength = ::GetLogicalDriveStrings(drivesBuffLen, drivesBuff);
	ParseDriveBuffer();
	EnumerateDriveTypes();

}

// private ==========================================

void LogicalDrivesEnumerator::ParseDriveBuffer() {

	std::wstring temp;

	for (int i = 0; i < bufferLength; i++)
	{
		temp.push_back(drivesBuff[i]);

		if (drivesBuff[i] == 0)
		{
			DriveNames.push_back(temp.c_str());
			temp.clear();
		}
	}
}

void LogicalDrivesEnumerator::EnumerateDriveTypes() {

	for (std::wstring driveName : DriveNames)
	{
		UINT driveType = ::GetDriveType(driveName.c_str());
		LogicalDrives.push_back(LOGICAL_DRIVE{ driveName, driveType });
	}
}