#include "PortableDeviceFileProviderPlugin.h"

#include "FileProvider.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSet>
#include <QTemporaryFile>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <atomic>
#include <functional>
#include <optional>
#include <vector>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include <PortableDeviceTypes.h>
#endif

namespace {

constexpr QLatin1StringView PortableRoot{"portable://"};
constexpr QLatin1StringView PortableDevicePrefix{"portable://device/"};
constexpr QLatin1StringView PortableObjectSegment{"/object/"};

const QSet<QString> kImageSuffixes = {
    QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"),
    QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("heic"), QStringLiteral("tif"),
    QStringLiteral("tiff")
};

const QSet<QString> kPreviewableMediaSuffixes = {
    QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"),
    QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("heic"), QStringLiteral("tif"),
    QStringLiteral("tiff"), QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("mkv"),
    QStringLiteral("avi"), QStringLiteral("webm")
};

struct PortablePath {
    bool valid = false;
    bool root = false;
    QString deviceId;
    QString objectId;
};

struct PortableDeviceInfo {
    QString deviceId;
    QString name;
    QString manufacturer;
    QString description;
    QString kind;
};

QString encodedSegment(const QString &value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

QString decodedSegment(const QString &value)
{
    return QUrl::fromPercentEncoding(value.toUtf8());
}

QString devicePath(const QString &deviceId)
{
    return QString(PortableDevicePrefix) + encodedSegment(deviceId);
}

QString objectPath(const QString &deviceId, const QString &objectId)
{
    return devicePath(deviceId) + QString(PortableObjectSegment) + encodedSegment(objectId);
}

PortablePath parsePortablePath(QString path)
{
    path = path.trimmed();
    if (path.compare(QStringLiteral("portable:"), Qt::CaseInsensitive) == 0
        || path.compare(QStringLiteral("portable:/"), Qt::CaseInsensitive) == 0
        || path.compare(QString(PortableRoot), Qt::CaseInsensitive) == 0) {
        return {true, true, {}, {}};
    }

    if (!path.startsWith(PortableDevicePrefix, Qt::CaseInsensitive)) {
        return {};
    }

    const QString tail = path.mid(QString(PortableDevicePrefix).size());
    if (tail.isEmpty()) {
        return {};
    }

    const int objectSegment = tail.indexOf(QString(PortableObjectSegment), 0, Qt::CaseInsensitive);
    if (objectSegment < 0) {
        return {true, false, decodedSegment(tail), {}};
    }

    const QString encodedDevice = tail.left(objectSegment);
    const QString encodedObject = tail.mid(objectSegment + QString(PortableObjectSegment).size());
    if (encodedDevice.isEmpty() || encodedObject.isEmpty()) {
        return {};
    }
    return {true, false, decodedSegment(encodedDevice), decodedSegment(encodedObject)};
}

QString normalizedPortablePath(const QString &path)
{
    const PortablePath parsed = parsePortablePath(path);
    if (!parsed.valid) {
        return {};
    }
    if (parsed.root) {
        return QString(PortableRoot);
    }
    if (parsed.objectId.isEmpty()) {
        return devicePath(parsed.deviceId);
    }
    return objectPath(parsed.deviceId, parsed.objectId);
}

QString suffixForName(const QString &name)
{
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0 || dot == name.size() - 1) {
        return {};
    }
    return name.mid(dot + 1).toLower();
}

QString fallbackDeviceName(const PortableDeviceInfo &device)
{
    if (!device.name.trimmed().isEmpty()) {
        return device.name.trimmed();
    }
    if (!device.description.trimmed().isEmpty()) {
        return device.description.trimmed();
    }
    if (!device.manufacturer.trimmed().isEmpty()) {
        return device.manufacturer.trimmed();
    }
    return QStringLiteral("Portable device");
}

FileEntry virtualDirectoryEntry(const QString &name,
                                const QString &path,
                                const QString &attributes = QStringLiteral("Read-only portable folder"))
{
    FileEntry entry;
    entry.name = name;
    entry.path = path;
    entry.attributesText = attributes;
    entry.isDirectory = true;
    entry.isReadOnly = true;
    return entry;
}

QString readOnlyError()
{
    return QStringLiteral("portable:// is read-only");
}

#ifdef Q_OS_WIN
template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr &) = delete;
    ComPtr &operator=(const ComPtr &) = delete;
    ComPtr(ComPtr &&other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }
    ComPtr &operator=(ComPtr &&other) noexcept
    {
        if (this != &other) {
            reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T *get() const { return m_ptr; }
    T **out()
    {
        reset();
        return &m_ptr;
    }
    T *operator->() const { return m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }
    void reset()
    {
        if (m_ptr) {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }

private:
    T *m_ptr = nullptr;
};

class ComInit {
public:
    ComInit()
    {
        m_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        m_uninitialize = m_result == S_OK || m_result == S_FALSE;
    }

    ~ComInit()
    {
        if (m_uninitialize) {
            CoUninitialize();
        }
    }

    bool ok() const
    {
        return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE;
    }

    HRESULT result() const { return m_result; }

private:
    HRESULT m_result = E_FAIL;
    bool m_uninitialize = false;
};

QString windowsError(HRESULT hr)
{
    wchar_t *messageBuffer = nullptr;
    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(hr),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&messageBuffer),
        0,
        nullptr);

    QString message = size > 0 && messageBuffer
        ? QString::fromWCharArray(messageBuffer, static_cast<int>(size)).trimmed()
        : QStringLiteral("Windows error 0x%1").arg(static_cast<qulonglong>(hr), 8, 16, QLatin1Char('0'));

    if (messageBuffer) {
        LocalFree(messageBuffer);
    }
    return message;
}

bool portableOpenFailureLooksRemoved(HRESULT hr, const QString &message)
{
    if (HRESULT_FACILITY(hr) == FACILITY_WIN32) {
        switch (HRESULT_CODE(hr)) {
        case ERROR_BAD_UNIT:
        case ERROR_GEN_FAILURE:
        case ERROR_DEV_NOT_EXIST:
        case ERROR_DEVICE_NOT_CONNECTED:
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return true;
        default:
            break;
        }
    }

    const QString lower = message.toLower();
    return lower.contains(QStringLiteral("removed"))
        || lower.contains(QStringLiteral("not connected"))
        || lower.contains(QStringLiteral("not functioning"))
        || lower.contains(QStringLiteral("does not exist"))
        || lower.contains(QStringLiteral("cannot find"))
        || lower.contains(QStringLiteral("no longer available"));
}

template <typename T>
HRESULT createComInstance(REFCLSID clsid, ComPtr<T> &ptr)
{
    return CoCreateInstance(clsid,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            __uuidof(T),
                            reinterpret_cast<void **>(ptr.out()));
}

QString managerString(IPortableDeviceManager *manager,
                      LPCWSTR deviceId,
                      HRESULT (STDMETHODCALLTYPE IPortableDeviceManager::*method)(LPCWSTR, WCHAR *, DWORD *))
{
    if (!manager || !deviceId) {
        return {};
    }

    DWORD length = 0;
    HRESULT hr = (manager->*method)(deviceId, nullptr, &length);
    if (length == 0) {
        return {};
    }

    std::vector<WCHAR> buffer(length + 1);
    hr = (manager->*method)(deviceId, buffer.data(), &length);
    if (FAILED(hr)) {
        return {};
    }
    return QString::fromWCharArray(buffer.data()).trimmed();
}

bool portableDeviceIdLooksDriveBacked(const QString &deviceId)
{
    return deviceId.contains(QStringLiteral("USBSTOR"), Qt::CaseInsensitive)
        || deviceId.contains(QStringLiteral("STORAGE#VOLUME"), Qt::CaseInsensitive)
        || deviceId.contains(QStringLiteral("{53F56307-B6BF-11D0-94F2-00A0C91EFB8B}"), Qt::CaseInsensitive);
}

QList<PortableDeviceInfo> enumeratePortableDevices()
{
    QList<PortableDeviceInfo> result;

    ComInit com;
    if (!com.ok()) {
        return result;
    }

    ComPtr<IPortableDeviceManager> manager;
    if (FAILED(createComInstance(CLSID_PortableDeviceManager, manager)) || !manager) {
        return result;
    }

    manager->RefreshDeviceList();
    DWORD count = 0;
    if (FAILED(manager->GetDevices(nullptr, &count)) || count == 0) {
        return result;
    }

    std::vector<LPWSTR> ids(count, nullptr);
    if (FAILED(manager->GetDevices(ids.data(), &count))) {
        return result;
    }

    for (DWORD i = 0; i < count; ++i) {
        if (!ids[i]) {
            continue;
        }

        PortableDeviceInfo info;
        info.deviceId = QString::fromWCharArray(ids[i]);
        if (portableDeviceIdLooksDriveBacked(info.deviceId)) {
            continue;
        }
        info.name = managerString(manager.get(), ids[i], &IPortableDeviceManager::GetDeviceFriendlyName);
        info.description = managerString(manager.get(), ids[i], &IPortableDeviceManager::GetDeviceDescription);
        info.manufacturer = managerString(manager.get(), ids[i], &IPortableDeviceManager::GetDeviceManufacturer);

        const QString combined = QStringLiteral("%1 %2 %3 %4")
            .arg(info.name, info.description, info.manufacturer, info.deviceId)
            .toLower();
        info.kind = combined.contains(QStringLiteral("ptp")) || combined.contains(QStringLiteral("camera"))
            ? QStringLiteral("photo")
            : QStringLiteral("portable");
        result.append(info);
    }

    for (LPWSTR id : ids) {
        CoTaskMemFree(id);
    }

    std::sort(result.begin(), result.end(), [](const PortableDeviceInfo &lhs, const PortableDeviceInfo &rhs) {
        return fallbackDeviceName(lhs).compare(fallbackDeviceName(rhs), Qt::CaseInsensitive) < 0;
    });
    return result;
}

std::optional<PortableDeviceInfo> deviceInfoForId(const QString &deviceId)
{
    for (const PortableDeviceInfo &device : enumeratePortableDevices()) {
        if (device.deviceId == deviceId) {
            return device;
        }
    }
    return std::nullopt;
}

bool openDevice(const QString &deviceId, ComPtr<IPortableDevice> &device, QString *error)
{
    ComPtr<IPortableDeviceValues> clientInfo;
    HRESULT hr = createComInstance(CLSID_PortableDeviceValues, clientInfo);
    if (FAILED(hr) || !clientInfo) {
        if (error) {
            *error = windowsError(hr);
        }
        return false;
    }

    clientInfo->SetStringValue(WPD_CLIENT_NAME, L"FMQml");
    clientInfo->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, 1);
    clientInfo->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, 0);

    hr = createComInstance(CLSID_PortableDevice, device);
    if (FAILED(hr) || !device) {
        if (error) {
            *error = windowsError(hr);
        }
        return false;
    }

    const std::wstring id = deviceId.toStdWString();
    hr = device->Open(id.c_str(), clientInfo.get());
    if (FAILED(hr)) {
        if (error) {
            const QString reason = windowsError(hr);
            *error = portableOpenFailureLooksRemoved(hr, reason)
                ? QStringLiteral("Portable device was removed. %1").arg(reason)
                : QStringLiteral("Cannot open portable device. Unlock the phone and allow file transfer. %1")
                    .arg(reason);
        }
        device.reset();
        return false;
    }

    return true;
}

bool openContent(const QString &deviceId,
                 ComPtr<IPortableDevice> &device,
                 ComPtr<IPortableDeviceContent> &content,
                 QString *error)
{
    if (!openDevice(deviceId, device, error)) {
        return false;
    }
    const HRESULT hr = device->Content(content.out());
    if (FAILED(hr) || !content) {
        if (error) {
            *error = windowsError(hr);
        }
        return false;
    }
    return true;
}

ComPtr<IPortableDeviceKeyCollection> metadataKeys()
{
    ComPtr<IPortableDeviceKeyCollection> keys;
    if (FAILED(createComInstance(CLSID_PortableDeviceKeyCollection, keys)) || !keys) {
        return keys;
    }
    keys->Add(WPD_OBJECT_ID);
    keys->Add(WPD_OBJECT_PARENT_ID);
    keys->Add(WPD_OBJECT_NAME);
    keys->Add(WPD_OBJECT_ORIGINAL_FILE_NAME);
    keys->Add(WPD_OBJECT_CONTENT_TYPE);
    keys->Add(WPD_OBJECT_FORMAT);
    keys->Add(WPD_OBJECT_SIZE);
    keys->Add(WPD_OBJECT_DATE_CREATED);
    keys->Add(WPD_OBJECT_DATE_MODIFIED);
    keys->Add(WPD_OBJECT_ISHIDDEN);
    keys->Add(WPD_OBJECT_ISSYSTEM);
    return keys;
}

QString stringValue(IPortableDeviceValues *values, REFPROPERTYKEY key)
{
    if (!values) {
        return {};
    }
    LPWSTR raw = nullptr;
    const HRESULT hr = values->GetStringValue(key, &raw);
    if (FAILED(hr) || !raw) {
        return {};
    }
    const QString result = QString::fromWCharArray(raw).trimmed();
    CoTaskMemFree(raw);
    return result;
}

qulonglong largeValue(IPortableDeviceValues *values, REFPROPERTYKEY key)
{
    if (!values) {
        return 0;
    }
    ULONGLONG value = 0;
    return SUCCEEDED(values->GetUnsignedLargeIntegerValue(key, &value)) ? value : 0;
}

bool boolValue(IPortableDeviceValues *values, REFPROPERTYKEY key)
{
    if (!values) {
        return false;
    }
    BOOL value = FALSE;
    return SUCCEEDED(values->GetBoolValue(key, &value)) && value;
}

GUID guidValue(IPortableDeviceValues *values, REFPROPERTYKEY key)
{
    GUID value = GUID_NULL;
    if (values) {
        values->GetGuidValue(key, &value);
    }
    return value;
}

QDateTime dateValue(IPortableDeviceValues *values, REFPROPERTYKEY key)
{
    const QString text = stringValue(values, key);
    if (text.isEmpty()) {
        return {};
    }
    QDateTime date = QDateTime::fromString(text, Qt::ISODate);
    if (!date.isValid()) {
        date = QDateTime::fromString(text, QStringLiteral("yyyyMMddThhmmss"));
    }
    return date;
}

bool guidEquals(REFGUID lhs, REFGUID rhs)
{
    return IsEqualGUID(lhs, rhs) != FALSE;
}

FileEntry entryFromValues(const QString &deviceId, const QString &objectId, IPortableDeviceValues *values)
{
    FileEntry entry;
    entry.path = objectPath(deviceId, objectId);
    entry.name = stringValue(values, WPD_OBJECT_ORIGINAL_FILE_NAME);
    if (entry.name.isEmpty()) {
        entry.name = stringValue(values, WPD_OBJECT_NAME);
    }
    if (entry.name.isEmpty()) {
        entry.name = objectId;
    }

    const GUID contentType = guidValue(values, WPD_OBJECT_CONTENT_TYPE);
    entry.isDirectory = guidEquals(contentType, WPD_CONTENT_TYPE_FOLDER)
        || guidEquals(contentType, WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT);
    entry.size = entry.isDirectory ? 0 : static_cast<qint64>(largeValue(values, WPD_OBJECT_SIZE));
    entry.suffix = suffixForName(entry.name);
    entry.isHidden = boolValue(values, WPD_OBJECT_ISHIDDEN);
    entry.isSystem = boolValue(values, WPD_OBJECT_ISSYSTEM);
    entry.isReadOnly = true;
    entry.modified = dateValue(values, WPD_OBJECT_DATE_MODIFIED);
    entry.created = dateValue(values, WPD_OBJECT_DATE_CREATED);
    if (!entry.created.isValid()) {
        entry.created = entry.modified;
    }
    entry.modifiedText = entry.modified.isValid() ? entry.modified.toLocalTime().toString(Qt::ISODate) : QString();
    entry.createdText = entry.created.isValid() ? entry.created.toLocalTime().toString(Qt::ISODate) : QString();
    entry.sizeText = entry.isDirectory || entry.size <= 0 ? QString() : QLocale().formattedDataSize(entry.size);
    entry.attributesText = entry.isDirectory ? QStringLiteral("Portable folder") : QStringLiteral("Read-only");
    entry.isImage = !entry.isDirectory && kImageSuffixes.contains(entry.suffix);
    entry.hasThumbnail = !entry.isDirectory && kPreviewableMediaSuffixes.contains(entry.suffix);
    if (guidEquals(contentType, WPD_CONTENT_TYPE_IMAGE)) {
        entry.mimeType = QStringLiteral("image/*");
        entry.isImage = true;
        entry.hasThumbnail = true;
    } else if (guidEquals(contentType, WPD_CONTENT_TYPE_VIDEO)) {
        entry.mimeType = QStringLiteral("video/*");
        entry.hasThumbnail = true;
    } else if (guidEquals(contentType, WPD_CONTENT_TYPE_AUDIO)) {
        entry.mimeType = QStringLiteral("audio/*");
    }
    return entry;
}

std::optional<FileEntry> objectEntryBlocking(const QString &deviceId,
                                             const QString &objectId,
                                             QString *parentPath,
                                             QString *error)
{
    ComInit com;
    if (!com.ok()) {
        if (error) {
            *error = windowsError(com.result());
        }
        return std::nullopt;
    }

    ComPtr<IPortableDevice> device;
    ComPtr<IPortableDeviceContent> content;
    if (!openContent(deviceId, device, content, error)) {
        return std::nullopt;
    }

    ComPtr<IPortableDeviceProperties> properties;
    HRESULT hr = content->Properties(properties.out());
    if (FAILED(hr) || !properties) {
        if (error) {
            *error = windowsError(hr);
        }
        return std::nullopt;
    }

    ComPtr<IPortableDeviceKeyCollection> keys = metadataKeys();
    ComPtr<IPortableDeviceValues> values;
    const std::wstring id = objectId.toStdWString();
    hr = properties->GetValues(id.c_str(), keys.get(), values.out());
    if (FAILED(hr) || !values) {
        if (error) {
            *error = windowsError(hr);
        }
        return std::nullopt;
    }

    if (parentPath) {
        const QString parentId = stringValue(values.get(), WPD_OBJECT_PARENT_ID);
        *parentPath = parentId.isEmpty() || parentId == QString::fromWCharArray(WPD_DEVICE_OBJECT_ID)
            ? devicePath(deviceId)
            : objectPath(deviceId, parentId);
    }
    return entryFromValues(deviceId, objectId, values.get());
}

QList<FileEntry> listDeviceObjectsBlocking(const QString &deviceId,
                                           const QString &parentObjectId,
                                           QString *error,
                                           QHash<QString, FileEntry> *entryCache,
                                           QHash<QString, QStringList> *childrenCache,
                                           QHash<QString, QString> *parentCache)
{
    QList<FileEntry> entries;

    ComInit com;
    if (!com.ok()) {
        if (error) {
            *error = windowsError(com.result());
        }
        return entries;
    }

    ComPtr<IPortableDevice> device;
    ComPtr<IPortableDeviceContent> content;
    if (!openContent(deviceId, device, content, error)) {
        return entries;
    }

    ComPtr<IEnumPortableDeviceObjectIDs> enumerator;
    const std::wstring parentId = parentObjectId.toStdWString();
    HRESULT hr = content->EnumObjects(0, parentId.c_str(), nullptr, enumerator.out());
    if (FAILED(hr) || !enumerator) {
        if (error) {
            *error = windowsError(hr);
        }
        return entries;
    }

    ComPtr<IPortableDeviceProperties> properties;
    hr = content->Properties(properties.out());
    if (FAILED(hr) || !properties) {
        if (error) {
            *error = windowsError(hr);
        }
        return entries;
    }

    ComPtr<IPortableDeviceKeyCollection> keys = metadataKeys();
    QStringList childPaths;
    for (;;) {
        LPWSTR rawObjectIds[16] = {};
        ULONG fetched = 0;
        hr = enumerator->Next(16, rawObjectIds, &fetched);
        if (FAILED(hr)) {
            if (error) {
                *error = windowsError(hr);
            }
            break;
        }
        if (fetched == 0) {
            break;
        }

        for (ULONG i = 0; i < fetched; ++i) {
            if (!rawObjectIds[i]) {
                continue;
            }
            const QString objectId = QString::fromWCharArray(rawObjectIds[i]);
            ComPtr<IPortableDeviceValues> values;
            const HRESULT valueHr = properties->GetValues(rawObjectIds[i], keys.get(), values.out());
            CoTaskMemFree(rawObjectIds[i]);
            if (FAILED(valueHr) || !values) {
                continue;
            }

            FileEntry entry = entryFromValues(deviceId, objectId, values.get());
            entries.append(entry);
            childPaths.append(entry.path);
            if (entryCache) {
                entryCache->insert(entry.path, entry);
            }
            if (parentCache) {
                const QString parentPathValue = parentObjectId == QString::fromWCharArray(WPD_DEVICE_OBJECT_ID)
                    ? devicePath(deviceId)
                    : objectPath(deviceId, parentObjectId);
                parentCache->insert(entry.path, parentPathValue);
            }
        }

        if (hr == S_FALSE) {
            break;
        }
    }

    if (childrenCache) {
        const QString parentPathValue = parentObjectId == QString::fromWCharArray(WPD_DEVICE_OBJECT_ID)
            ? devicePath(deviceId)
            : objectPath(deviceId, parentObjectId);
        childrenCache->insert(parentPathValue, childPaths);
    }

    return entries;
}

bool copyObjectToLocalFileBlocking(const QString &deviceId,
                                   const QString &objectId,
                                   const QString &destinationFilePath,
                                   qint64 totalBytes,
                                   const std::function<bool(qint64, qint64)> &progress,
                                   QString *error)
{
    ComInit com;
    if (!com.ok()) {
        if (error) {
            *error = windowsError(com.result());
        }
        return false;
    }

    ComPtr<IPortableDevice> device;
    ComPtr<IPortableDeviceContent> content;
    if (!openContent(deviceId, device, content, error)) {
        return false;
    }

    ComPtr<IPortableDeviceResources> resources;
    HRESULT hr = content->Transfer(resources.out());
    if (FAILED(hr) || !resources) {
        if (error) {
            *error = windowsError(hr);
        }
        return false;
    }

    DWORD optimalBufferSize = 0;
    ComPtr<IStream> stream;
    const std::wstring id = objectId.toStdWString();
    hr = resources->GetStream(id.c_str(), WPD_RESOURCE_DEFAULT, STGM_READ, &optimalBufferSize, stream.out());
    if (FAILED(hr) || !stream) {
        if (error) {
            *error = windowsError(hr);
        }
        return false;
    }

    QFile output(destinationFilePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Cannot open destination file: %1").arg(output.errorString());
        }
        return false;
    }

    const qsizetype bufferSize = std::clamp<qsizetype>(optimalBufferSize > 0 ? optimalBufferSize : 64 * 1024,
                                                       16 * 1024,
                                                       1024 * 1024);
    QByteArray buffer;
    buffer.resize(static_cast<int>(bufferSize));

    qint64 processed = 0;
    for (;;) {
        ULONG bytesRead = 0;
        hr = stream->Read(buffer.data(), static_cast<ULONG>(buffer.size()), &bytesRead);
        if (FAILED(hr)) {
            output.close();
            output.remove();
            if (error) {
                *error = windowsError(hr);
            }
            return false;
        }
        if (bytesRead == 0) {
            break;
        }
        if (output.write(buffer.constData(), static_cast<qint64>(bytesRead)) != static_cast<qint64>(bytesRead)) {
            const QString writeError = output.errorString();
            output.close();
            output.remove();
            if (error) {
                *error = QStringLiteral("Portable device copy write failed: %1").arg(writeError);
            }
            return false;
        }
        processed += static_cast<qint64>(bytesRead);
        if (progress && !progress(processed, totalBytes)) {
            output.close();
            output.remove();
            if (error) {
                *error = QStringLiteral("Portable device transfer canceled");
            }
            return false;
        }
    }

    output.close();
    if (error) {
        error->clear();
    }
    return true;
}
#else
QList<PortableDeviceInfo> enumeratePortableDevices()
{
    return {};
}
#endif

class PortableDeviceFileProvider final : public FileProvider
{
public:
    explicit PortableDeviceFileProvider(QObject *parent = nullptr)
        : FileProvider(parent)
    {
    }

    QString scheme() const override { return QStringLiteral("portable"); }
    bool canHandle(const QString &path) const override { return !normalizedPortablePath(path).isEmpty(); }
    Capabilities capabilities() const override { return Browse | ReadMetadata | Transfer; }

    void scan(const QString &path) override
    {
        clearLastError();
        const QString normalized = normalizedPath(path);
        const int generation = m_generation.fetch_add(1) + 1;
        m_currentPath = normalized;
        m_running.store(true);
        emit started();

        if (normalized.isEmpty()) {
            finish(generation, normalized, false, QStringLiteral("Portable device path is invalid"));
            return;
        }

        const bool showHidden = m_showHidden;
        QPointer<PortableDeviceFileProvider> self(this);
        auto future = QtConcurrent::run([self, normalized, generation, showHidden]() {
            QString error;
            QList<FileEntry> entries = self
                ? self->listEntriesBlocking(normalized, showHidden, &error)
                : QList<FileEntry>{};
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self.data(), [self, normalized, generation, entries = std::move(entries), error]() mutable {
                if (!self || generation != self->m_generation.load()) {
                    return;
                }
                if (!entries.isEmpty()) {
                    emit self->batchReady(entries, generation);
                }
                self->finish(generation, normalized, error.isEmpty(), error);
            }, Qt::QueuedConnection);
        });
        Q_UNUSED(future)
    }

    void cancel() override
    {
        m_generation.fetch_add(1);
        m_running.store(false);
    }

    void setShowHidden(bool show) override { m_showHidden = show; }
    bool isRunning() const override { return m_running.load(); }
    QString currentPath() const override { return m_currentPath; }
    int currentGeneration() const override { return m_generation.load(); }

    bool pathExists(const QString &path) const override
    {
        const QString normalized = normalizedPath(path);
        const PortablePath parsed = parsePortablePath(normalized);
        if (!parsed.valid) {
            return false;
        }
        if (parsed.root) {
            return true;
        }
        if (parsed.objectId.isEmpty()) {
            return deviceInfoForId(parsed.deviceId).has_value();
        }
        return entryInfo(normalized).has_value();
    }

    bool isDirectory(const QString &path) const override
    {
        const QString normalized = normalizedPath(path);
        const PortablePath parsed = parsePortablePath(normalized);
        if (!parsed.valid) {
            return false;
        }
        if (parsed.root || parsed.objectId.isEmpty()) {
            return true;
        }
        const std::optional<FileEntry> entry = entryInfo(normalized);
        return entry && entry->isDirectory;
    }

    bool isSymLink(const QString &) const override { return false; }
    QString normalizedPath(const QString &path) const override { return normalizedPortablePath(path); }
    QString absolutePath(const QString &path) const override { return normalizedPath(path); }

    QString fileName(const QString &path) const override
    {
        const QString normalized = normalizedPath(path);
        const PortablePath parsed = parsePortablePath(normalized);
        if (!parsed.valid) {
            return {};
        }
        if (parsed.root) {
            return QStringLiteral("Portable media devices");
        }
        if (parsed.objectId.isEmpty()) {
            const auto info = deviceInfoForId(parsed.deviceId);
            return info ? fallbackDeviceName(*info) : QStringLiteral("Portable device");
        }
        const std::optional<FileEntry> entry = entryInfo(normalized);
        return entry ? entry->name : parsed.objectId;
    }

    QString parentPath(const QString &path) const override
    {
        const QString normalized = normalizedPath(path);
        const PortablePath parsed = parsePortablePath(normalized);
        if (!parsed.valid || parsed.root) {
            return {};
        }
        if (parsed.objectId.isEmpty()) {
            return QString(PortableRoot);
        }

        QMutexLocker locker(&m_cacheMutex);
        const auto it = m_parentByPath.constFind(normalized);
        if (it != m_parentByPath.cend()) {
            return it.value();
        }
        locker.unlock();

        QString parent;
        objectEntryBlocking(parsed.deviceId, parsed.objectId, &parent, nullptr);
        if (!parent.isEmpty()) {
            QMutexLocker cacheLocker(&m_cacheMutex);
            m_parentByPath.insert(normalized, parent);
            return parent;
        }
        return devicePath(parsed.deviceId);
    }

    QString childPath(const QString &parentPathValue, const QString &name) const override
    {
        const QString normalizedParent = normalizedPath(parentPathValue);
        const QString cleanName = name.trimmed();
        if (normalizedParent.isEmpty() || cleanName.isEmpty()) {
            return {};
        }
        const QStringList children = childPaths(normalizedParent, true);
        for (const QString &child : children) {
            if (fileName(child).compare(cleanName, Qt::CaseInsensitive) == 0) {
                return child;
            }
        }
        return {};
    }

    std::optional<FileEntry> entryInfo(const QString &path) const override
    {
        const QString normalized = normalizedPath(path);
        const PortablePath parsed = parsePortablePath(normalized);
        if (!parsed.valid) {
            return std::nullopt;
        }
        if (parsed.root) {
            return virtualDirectoryEntry(QStringLiteral("Portable media devices"), QString(PortableRoot));
        }
        if (parsed.objectId.isEmpty()) {
            const auto info = deviceInfoForId(parsed.deviceId);
            if (!info) {
                return std::nullopt;
            }
            return virtualDirectoryEntry(fallbackDeviceName(*info),
                                         devicePath(parsed.deviceId),
                                         QStringLiteral("Read-only portable media device"));
        }

        {
            QMutexLocker locker(&m_cacheMutex);
            const auto it = m_entryByPath.constFind(normalized);
            if (it != m_entryByPath.cend()) {
                return it.value();
            }
        }

#ifdef Q_OS_WIN
        QString parent;
        QString error;
        const std::optional<FileEntry> entry = objectEntryBlocking(parsed.deviceId, parsed.objectId, &parent, &error);
        if (entry) {
            QMutexLocker locker(&m_cacheMutex);
            m_entryByPath.insert(entry->path, *entry);
            if (!parent.isEmpty()) {
                m_parentByPath.insert(entry->path, parent);
            }
        } else if (!error.isEmpty()) {
            setLastError(error);
        }
        return entry;
#else
        return std::nullopt;
#endif
    }

    bool ensureParentDirectory(const QString &) const override { return failReadOnly(); }
    bool makePath(const QString &) const override { return failReadOnly(); }
    bool removePath(const QString &) const override { return failReadOnly(); }
    bool movePath(const QString &, const QString &) const override { return failReadOnly(); }

    QStringList childPaths(const QString &path, bool includeHidden = true) const override
    {
        const QString normalized = normalizedPath(path);
        {
            QMutexLocker locker(&m_cacheMutex);
            const auto it = m_childrenByPath.constFind(normalized);
            if (it != m_childrenByPath.cend()) {
                return includeHidden ? it.value() : visibleChildren(it.value());
            }
        }

        QString error;
        listEntriesBlocking(normalized, true, &error);
        if (!error.isEmpty()) {
            setLastError(error);
        }

        QMutexLocker locker(&m_cacheMutex);
        const auto it = m_childrenByPath.constFind(normalized);
        if (it == m_childrenByPath.cend()) {
            return {};
        }
        return includeHidden ? it.value() : visibleChildren(it.value());
    }

    std::unique_ptr<QIODevice> openRead(const QString &path) const override
    {
        return openRead(path, QString{});
    }

    std::unique_ptr<QIODevice> openRead(const QString &path, const QString &stagingParentPath) const override
    {
        clearLastError();
        const std::optional<FileEntry> entry = entryInfo(path);
        if (!entry || entry->isDirectory) {
            setLastError(QStringLiteral("Portable device file is not available"));
            return nullptr;
        }

        QString dirPath = stagingParentPath.trimmed();
        if (dirPath.isEmpty()) {
            dirPath = QDir::tempPath();
        }
        QDir().mkpath(dirPath);

        const QString suffix = entry->suffix.isEmpty() ? QStringLiteral("tmp") : entry->suffix;
        auto temp = std::make_unique<QTemporaryFile>(QDir(dirPath).filePath(QStringLiteral("fm-portable-XXXXXX.%1").arg(suffix)));
        temp->setAutoRemove(true);
        if (!temp->open()) {
            setLastError(QStringLiteral("Cannot create temporary preview file: %1").arg(temp->errorString()));
            return nullptr;
        }
        const QString tempPath = temp->fileName();
        temp->close();

        QString error;
        if (!copyToLocalFile(path, tempPath, {}, &error)) {
            setLastError(error);
            return nullptr;
        }

        if (!temp->open()) {
            setLastError(QStringLiteral("Cannot open temporary preview file: %1").arg(temp->errorString()));
            return nullptr;
        }
        return temp;
    }

    bool copyToLocalFile(const QString &sourcePath,
                         const QString &destinationFilePath,
                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                         QString *error) const override
    {
        clearLastError();
        const QString normalized = normalizedPath(sourcePath);
        const PortablePath parsed = parsePortablePath(normalized);
        if (!parsed.valid || parsed.objectId.isEmpty()) {
            const QString message = QStringLiteral("Portable device source path is invalid");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }

        const std::optional<FileEntry> entry = entryInfo(normalized);
        if (!entry || entry->isDirectory) {
            const QString message = QStringLiteral("Portable device file is not available");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }

#ifdef Q_OS_WIN
        QString copyError;
        const bool copied = copyObjectToLocalFileBlocking(parsed.deviceId,
                                                          parsed.objectId,
                                                          destinationFilePath,
                                                          entry->size,
                                                          progress,
                                                          &copyError);
        if (!copied) {
            setLastError(copyError);
            if (error) {
                *error = copyError;
            }
            return false;
        }
        if (error) {
            error->clear();
        }
        return true;
#else
        const QString message = QStringLiteral("Portable devices are not supported on this platform");
        setLastError(message);
        if (error) {
            *error = message;
        }
        return false;
#endif
    }

    bool copyFromLocalFile(const QString &, const QString &, const std::function<bool(qint64, qint64)> &, QString *error) const override
    {
        const QString message = readOnlyError();
        setLastError(message);
        if (error) {
            *error = message;
        }
        return false;
    }

    std::unique_ptr<QIODevice> openWrite(const QString &, bool truncate = true) const override
    {
        Q_UNUSED(truncate)
        failReadOnly();
        return nullptr;
    }

    bool renamePath(const QString &, const QString &) override { return failReadOnly(); }
    bool createFolder(const QString &, const QString &, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        return failReadOnly();
    }
    bool createFile(const QString &, const QString &, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        return failReadOnly();
    }

    QString lastErrorString() const override { return m_lastError; }
    void clearLastError() const override { m_lastError.clear(); }

private:
    void finish(int generation, const QString &path, bool success, const QString &error)
    {
        if (generation != m_generation.load()) {
            return;
        }
        m_running.store(false);
        if (!success) {
            setLastError(error);
        }
        emit finished(path, success, generation, error);
    }

    QList<FileEntry> listEntriesBlocking(const QString &path, bool showHidden, QString *error) const
    {
        const PortablePath parsed = parsePortablePath(path);
        if (!parsed.valid) {
            if (error) {
                *error = QStringLiteral("Portable device path is invalid");
            }
            return {};
        }

        if (parsed.root) {
            QList<FileEntry> entries;
            QStringList children;
            const QList<PortableDeviceInfo> devices = enumeratePortableDevices();
            for (const PortableDeviceInfo &device : devices) {
                const QString pathValue = devicePath(device.deviceId);
                FileEntry entry = virtualDirectoryEntry(fallbackDeviceName(device),
                                                        pathValue,
                                                        device.kind == QLatin1String("photo")
                                                            ? QStringLiteral("Read-only camera/photo device")
                                                            : QStringLiteral("Read-only portable media device"));
                entry.providerCapabilitiesText = QStringLiteral("Read-only");
                entries.append(entry);
                children.append(pathValue);
                QMutexLocker locker(&m_cacheMutex);
                m_entryByPath.insert(pathValue, entry);
                m_parentByPath.insert(pathValue, QString(PortableRoot));
            }
            QMutexLocker locker(&m_cacheMutex);
            m_childrenByPath.insert(QString(PortableRoot), children);
            if (error) {
                error->clear();
            }
            return entries;
        }

        if (!deviceInfoForId(parsed.deviceId)) {
            if (error) {
                *error = QStringLiteral("Portable device was removed");
            }
            return {};
        }

        const QString parentObjectId = parsed.objectId.isEmpty()
            ? QString::fromWCharArray(WPD_DEVICE_OBJECT_ID)
            : parsed.objectId;

#ifdef Q_OS_WIN
        QHash<QString, FileEntry> entries;
        QHash<QString, QStringList> children;
        QHash<QString, QString> parents;
        QList<FileEntry> listed = listDeviceObjectsBlocking(parsed.deviceId,
                                                            parentObjectId,
                                                            error,
                                                            &entries,
                                                            &children,
                                                            &parents);
        if (!showHidden) {
            listed.erase(std::remove_if(listed.begin(), listed.end(), [](const FileEntry &entry) {
                return entry.isHidden;
            }), listed.end());
        }
        QMutexLocker locker(&m_cacheMutex);
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            m_entryByPath.insert(it.key(), it.value());
        }
        for (auto it = children.cbegin(); it != children.cend(); ++it) {
            m_childrenByPath.insert(it.key(), it.value());
        }
        for (auto it = parents.cbegin(); it != parents.cend(); ++it) {
            m_parentByPath.insert(it.key(), it.value());
        }
        return listed;
#else
        if (error) {
            *error = QStringLiteral("Portable devices are not supported on this platform");
        }
        return {};
#endif
    }

    QStringList visibleChildren(const QStringList &children) const
    {
        QStringList result;
        result.reserve(children.size());
        for (const QString &child : children) {
            const auto entry = entryInfo(child);
            if (entry && !entry->isHidden) {
                result.append(child);
            }
        }
        return result;
    }

    bool failReadOnly() const
    {
        setLastError(readOnlyError());
        return false;
    }

    void setLastError(const QString &error) const
    {
        m_lastError = error;
    }

    QString m_currentPath = QString(PortableRoot);
    std::atomic<int> m_generation{0};
    std::atomic_bool m_running{false};
    bool m_showHidden = false;
    mutable QString m_lastError;
    mutable QMutex m_cacheMutex;
    mutable QHash<QString, FileEntry> m_entryByPath;
    mutable QHash<QString, QStringList> m_childrenByPath;
    mutable QHash<QString, QString> m_parentByPath;
};

} // namespace

int PortableDeviceFileProviderPlugin::apiVersion() const
{
    return FM_FILE_PROVIDER_PLUGIN_API_VERSION;
}

QString PortableDeviceFileProviderPlugin::pluginId() const
{
    return QStringLiteral("fm.portable-device-provider");
}

QString PortableDeviceFileProviderPlugin::displayName() const
{
    return QStringLiteral("Portable Device Provider");
}

QStringList PortableDeviceFileProviderPlugin::schemes() const
{
    return {QStringLiteral("portable")};
}

bool PortableDeviceFileProviderPlugin::canHandle(const QString &path) const
{
    return !normalizedPortablePath(path).isEmpty();
}

std::unique_ptr<FileProvider> PortableDeviceFileProviderPlugin::createProvider()
{
    return std::make_unique<PortableDeviceFileProvider>();
}

int PortableDeviceFileProviderPlugin::placesApiVersion() const
{
    return FM_PLACES_PROVIDER_PLUGIN_API_VERSION;
}

QString PortableDeviceFileProviderPlugin::placesPluginId() const
{
    return pluginId();
}

QString PortableDeviceFileProviderPlugin::placesDisplayName() const
{
    return displayName();
}

QList<ProviderPlaceItem> PortableDeviceFileProviderPlugin::places() const
{
    QList<ProviderPlaceItem> result;
    const QList<PortableDeviceInfo> devices = enumeratePortableDevices();
    result.reserve(devices.size());
    for (const PortableDeviceInfo &device : devices) {
        ProviderPlaceItem item;
        item.name = fallbackDeviceName(device);
        item.path = devicePath(device.deviceId);
        item.icon = QStringLiteral("drive");
        item.section = QStringLiteral("portable");
        item.driveType = device.kind == QLatin1String("photo")
            ? QStringLiteral("camera")
            : QStringLiteral("portable");
        item.subtitle = device.kind == QLatin1String("photo")
            ? QStringLiteral("Read-only camera/photo device")
            : QStringLiteral("Read-only portable media device");
        item.isReady = true;
        item.canEject = false;
        result.append(item);
    }
    return result;
}
