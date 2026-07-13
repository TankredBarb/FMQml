#include "OperationQueue.h"
#include "OperationQueuePrivate.h"

#include <QFileInfo>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#include <windows.h>
#include <winioctl.h>
#endif

OperationQueue::DriveStorageType OperationQueue::getDriveTypeByPath(const QString &filePath)
{
#if defined(Q_OS_WIN)
    QString root = QFileInfo(filePath).absoluteDir().rootPath();
    if (root.isEmpty()) {
        return DriveStorageType::Unknown;
    }

    std::wstring stdRoot = root.toStdWString();
    LPCWSTR driveRoot = stdRoot.c_str();

    UINT winDriveType = GetDriveTypeW(driveRoot);
    if (winDriveType == DRIVE_REMOVABLE) {
        return DriveStorageType::USB_Flash;
    }
    if (winDriveType != DRIVE_FIXED) {
        return DriveStorageType::Unknown;
    }

    QString volumePath = QString(R"(\\.\)") + root.left(2);
    HANDLE hDevice = CreateFileW(
        volumePath.toStdWString().c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        return DriveStorageType::Unknown;
    }

    DriveStorageType detectedType = DriveStorageType::HDD;

    STORAGE_PROPERTY_QUERY query;
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR seekPenaltyDesc = {0};
    DWORD bytesReturned = 0;

    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query),
        &seekPenaltyDesc, sizeof(seekPenaltyDesc),
        &bytesReturned, NULL
    );

    if (result && !seekPenaltyDesc.IncursSeekPenalty) {
        detectedType = DriveStorageType::SATA_SSD;

        query.PropertyId = StorageAdapterProperty;
        query.QueryType = PropertyStandardQuery;

        STORAGE_ADAPTER_DESCRIPTOR adapterDesc = {0};
        result = DeviceIoControl(
            hDevice,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query, sizeof(query),
            &adapterDesc, sizeof(adapterDesc),
            &bytesReturned, NULL
        );

        if (result) {
            if (adapterDesc.BusType == BusTypeNvme) {
                detectedType = DriveStorageType::NVME_SSD;
            } else if (adapterDesc.BusType == BusTypeUsb) {
                detectedType = DriveStorageType::USB_Flash;
            }
        }
    }

    CloseHandle(hDevice);
    return detectedType;
#else
    Q_UNUSED(filePath);
    return DriveStorageType::Unknown;
#endif
}



qint64 OperationQueuePrivate::bufferSizeForStorageType(OperationQueue::DriveStorageType type)
{
    switch (type) {
    case OperationQueue::DriveStorageType::HDD:
    case OperationQueue::DriveStorageType::USB_Flash:
        return 512 * 1024;
    case OperationQueue::DriveStorageType::SATA_SSD:
        return 4 * 1024 * 1024;
    case OperationQueue::DriveStorageType::NVME_SSD:
        return 8 * 1024 * 1024;
    case OperationQueue::DriveStorageType::Unknown:
    default:
        return 1 * 1024 * 1024;
    }
}

