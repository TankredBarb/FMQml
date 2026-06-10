#include "FileAccessResolver.h"
#ifndef FM_ACCESS_RESOLVER_LOCAL_ONLY
#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#endif

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QVariantMap>

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
#include <aclapi.h>
#include <AccCtrl.h>
#include <authz.h>
#endif

namespace {

struct CacheEntry {
    FileCapabilityInfo info;
    qint64 size = -1;
    qint64 lastModifiedMs = -1;
    bool exists = false;
    qint64 cachedAtMs = 0;
};

QMutex &cacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, CacheEntry> &cacheStore()
{
    static QHash<QString, CacheEntry> cache;
    return cache;
}

QString cacheKeyForPath(const QString &path)
{
    QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    key = key.toLower();
#endif
    return key;
}

qint64 lastModifiedStamp(const QFileInfo &info)
{
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : -1;
}

bool isAllowed(FileAccessInfo::State state)
{
    return state == FileAccessInfo::State::Allowed;
}

QString accessStateKey(FileAccessInfo::State state)
{
    switch (state) {
    case FileAccessInfo::State::Allowed:
        return QStringLiteral("allowed");
    case FileAccessInfo::State::Denied:
        return QStringLiteral("denied");
    case FileAccessInfo::State::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

FileAccessInfo::State accessStateFromBool(bool allowed)
{
    return allowed ? FileAccessInfo::State::Allowed : FileAccessInfo::State::Denied;
}

FileAccessInfo::State accessStateFromOptional(const std::optional<bool> &allowed)
{
    if (!allowed.has_value()) {
        return FileAccessInfo::State::Unknown;
    }
    return accessStateFromBool(*allowed);
}

bool hasUnknownAccessState(const FileCapabilityInfo &info)
{
    if (info.isDirectory) {
        return info.access.browseState == FileAccessInfo::State::Unknown
            || info.access.createChildrenState == FileAccessInfo::State::Unknown
            || info.access.deleteState == FileAccessInfo::State::Unknown
            || info.access.traverseState == FileAccessInfo::State::Unknown
            || info.access.changeAttributesState == FileAccessInfo::State::Unknown;
    }
    return info.access.readState == FileAccessInfo::State::Unknown
        || info.access.modifyState == FileAccessInfo::State::Unknown
        || info.access.deleteState == FileAccessInfo::State::Unknown
        || info.access.executeState == FileAccessInfo::State::Unknown
        || info.access.changeAttributesState == FileAccessInfo::State::Unknown;
}

QString formatAccessSummary(const FileCapabilityInfo &info)
{
    QStringList items;
    const bool hasUnknown = hasUnknownAccessState(info);
    if (info.isDirectory) {
        if (info.access.canBrowse) {
            items.append(QStringLiteral("Browse"));
        }
        if (info.access.canCreateChildren) {
            items.append(QStringLiteral("Create inside"));
        }
        if (info.access.canDelete) {
            items.append(QStringLiteral("Delete"));
        }
        if (info.access.canTraverse) {
            items.append(QStringLiteral("Traverse"));
        }
    } else {
        if (info.access.canRead) {
            items.append(QStringLiteral("Read"));
        }
        if (info.access.canModify) {
            items.append(QStringLiteral("Modify"));
        }
        if (info.access.canDelete) {
            items.append(QStringLiteral("Delete"));
        }
        if (info.access.canExecute) {
            items.append(QStringLiteral("Execute"));
        }
    }

    if (items.isEmpty()) {
        return hasUnknown ? QStringLiteral("Access unknown") : QStringLiteral("No access");
    }
    if (hasUnknown) {
        items.append(QStringLiteral("Some access unknown"));
    }
    return items.join(QStringLiteral(", "));
}

QString formatAttributesSummary(const FileCapabilityInfo &info)
{
    QStringList items;
    if (info.attributes.hidden) {
        items.append(QStringLiteral("Hidden"));
    }
    if (info.attributes.readOnly) {
        items.append(QStringLiteral("Read-only"));
    }
    if (info.attributes.system) {
        items.append(QStringLiteral("System"));
    }
    return items.join(QStringLiteral(", "));
}

QVariantMap makeProperty(const QString &label, FileAccessInfo::State state, const QString &reason = QString())
{
    QVariantMap map;
    const bool allowed = isAllowed(state);
    map.insert(QStringLiteral("label"), label);
    map.insert(QStringLiteral("value"), state == FileAccessInfo::State::Unknown
                   ? QStringLiteral("Unknown")
                   : (allowed ? QStringLiteral("Allowed") : QStringLiteral("Unavailable")));
    map.insert(QStringLiteral("allowed"), allowed);
    map.insert(QStringLiteral("state"), accessStateKey(state));
    QString resolvedReason = reason;
    if (resolvedReason.isEmpty() && state == FileAccessInfo::State::Unknown) {
        resolvedReason = QStringLiteral("Effective access could not be verified.");
    }
    if (!resolvedReason.isEmpty()) {
        map.insert(QStringLiteral("reason"), resolvedReason);
    }
    return map;
}

QVariantMap makeProperty(const QString &label, bool allowed)
{
    return makeProperty(label, accessStateFromBool(allowed));
}

QVariantMap makeAttributeProperty(const QString &label, bool enabled)
{
    QVariantMap map;
    map.insert(QStringLiteral("label"), label);
    map.insert(QStringLiteral("value"), enabled ? QStringLiteral("Yes") : QStringLiteral("No"));
    map.insert(QStringLiteral("enabled"), enabled);
    return map;
}

#ifdef Q_OS_WIN
QString lastWindowsErrorString(DWORD errorCode)
{
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags,
                                        nullptr,
                                        errorCode,
                                        0,
                                        reinterpret_cast<LPWSTR>(&buffer),
                                        0,
                                        nullptr);
    QString detail;
    if (length > 0 && buffer) {
        detail = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
        LocalFree(buffer);
    }
    return detail.isEmpty() ? QStringLiteral("Windows error %1").arg(errorCode) : detail;
}

struct ScopedHandle {
    HANDLE handle = nullptr;
    ~ScopedHandle() {
        if (handle && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
};

struct ScopedSecurityDescriptor {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    ~ScopedSecurityDescriptor() {
        if (descriptor) {
            LocalFree(descriptor);
        }
    }
};

struct MandatoryLabelInfo {
    DWORD integrityRid = SECURITY_MANDATORY_MEDIUM_RID;
    ACCESS_MASK policy = SYSTEM_MANDATORY_LABEL_NO_WRITE_UP;
    bool exact = true;
};

struct AuthzRuntime {
    AUTHZ_RESOURCE_MANAGER_HANDLE resourceManager = nullptr;
    AUTHZ_CLIENT_CONTEXT_HANDLE clientContext = nullptr;
    ScopedHandle processToken;
    ScopedHandle impersonationToken;
    DWORD tokenIntegrityRid = SECURITY_MANDATORY_MEDIUM_RID;
    bool tokenIntegrityExact = false;
    bool authzReady = false;
    bool accessCheckReady = false;

    AuthzRuntime()
    {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &processToken.handle)) {
            return;
        }

        DWORD tokenInfoLength = 0;
        GetTokenInformation(processToken.handle, TokenIntegrityLevel, nullptr, 0, &tokenInfoLength);
        if (tokenInfoLength > 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            std::vector<BYTE> tokenInfo(tokenInfoLength);
            if (GetTokenInformation(processToken.handle,
                                    TokenIntegrityLevel,
                                    tokenInfo.data(),
                                    tokenInfoLength,
                                    &tokenInfoLength)) {
                const auto *label = reinterpret_cast<const TOKEN_MANDATORY_LABEL *>(tokenInfo.data());
                if (label->Label.Sid && IsValidSid(label->Label.Sid)) {
                    const UCHAR subAuthorityCount = *GetSidSubAuthorityCount(label->Label.Sid);
                    if (subAuthorityCount > 0) {
                        tokenIntegrityRid = *GetSidSubAuthority(label->Label.Sid, subAuthorityCount - 1);
                        tokenIntegrityExact = true;
                    }
                }
            }
        }

        if (!DuplicateToken(processToken.handle, SecurityIdentification, &impersonationToken.handle)) {
            return;
        }
        accessCheckReady = true;

        if (!AuthzInitializeResourceManager(AUTHZ_RM_FLAG_NO_AUDIT,
                                            nullptr,
                                            nullptr,
                                            nullptr,
                                            L"FM File Access Resolver",
                                            &resourceManager)) {
            return;
        }

        LUID luid{};
        if (!AuthzInitializeContextFromToken(0,
                                             processToken.handle,
                                             resourceManager,
                                             nullptr,
                                             luid,
                                             nullptr,
                                             &clientContext)) {
            return;
        }

        authzReady = true;
    }

    ~AuthzRuntime()
    {
        if (clientContext) {
            AuthzFreeContext(clientContext);
        }
        if (resourceManager) {
            AuthzFreeResourceManager(resourceManager);
        }
    }
};

FileAttributesInfo readWindowsAttributes(const QString &path, const QFileInfo &info);

DWORD windowsAttributesValue(const QString &path)
{
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    return GetFileAttributesW(nativePath.c_str());
}

AuthzRuntime &authzRuntime()
{
    static AuthzRuntime runtime;
    return runtime;
}

bool loadSecurityDescriptorWindows(const QString &path, ScopedSecurityDescriptor *securityDescriptor)
{
    if (!securityDescriptor) {
        return false;
    }

    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const DWORD result = GetNamedSecurityInfoW(const_cast<LPWSTR>(nativePath.c_str()),
                                               SE_FILE_OBJECT,
                                               OWNER_SECURITY_INFORMATION
                                                   | GROUP_SECURITY_INFORMATION
                                                   | DACL_SECURITY_INFORMATION,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               &descriptor);
    if (result != ERROR_SUCCESS || !descriptor) {
        return false;
    }

    securityDescriptor->descriptor = descriptor;
    return true;
}

MandatoryLabelInfo mandatoryLabelWindows(const QString &path)
{
    MandatoryLabelInfo label;

    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    PACL sacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const DWORD result = GetNamedSecurityInfoW(const_cast<LPWSTR>(nativePath.c_str()),
                                               SE_FILE_OBJECT,
                                               LABEL_SECURITY_INFORMATION,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               &sacl,
                                               &descriptor);
    ScopedSecurityDescriptor securityDescriptor;
    securityDescriptor.descriptor = descriptor;

    if (result != ERROR_SUCCESS) {
        label.exact = false;
        return label;
    }

    if (!sacl) {
        return label;
    }

    for (DWORD i = 0; i < sacl->AceCount; ++i) {
        void *acePointer = nullptr;
        if (!GetAce(sacl, i, &acePointer) || !acePointer) {
            continue;
        }

        const auto *header = static_cast<const ACE_HEADER *>(acePointer);
        if (header->AceType != SYSTEM_MANDATORY_LABEL_ACE_TYPE) {
            continue;
        }

        const auto *mandatoryAce = static_cast<const SYSTEM_MANDATORY_LABEL_ACE *>(acePointer);
        PSID sid = const_cast<DWORD *>(&mandatoryAce->SidStart);
        if (!sid || !IsValidSid(sid)) {
            label.exact = false;
            return label;
        }

        const UCHAR subAuthorityCount = *GetSidSubAuthorityCount(sid);
        if (subAuthorityCount == 0) {
            label.exact = false;
            return label;
        }

        label.integrityRid = *GetSidSubAuthority(sid, subAuthorityCount - 1);
        label.policy = mandatoryAce->Mask;
        return label;
    }

    return label;
}

std::optional<bool> authzAllowsWindows(PSECURITY_DESCRIPTOR securityDescriptor, ACCESS_MASK desiredAccess)
{
    AuthzRuntime &runtime = authzRuntime();
    if (!runtime.authzReady || !securityDescriptor) {
        return std::nullopt;
    }

    AUTHZ_ACCESS_REQUEST request{};
    request.DesiredAccess = desiredAccess;

    ACCESS_MASK grantedAccess = 0;
    DWORD saclEvaluation = 0;
    DWORD error = ERROR_SUCCESS;
    AUTHZ_ACCESS_REPLY reply{};
    reply.ResultListLength = 1;
    reply.GrantedAccessMask = &grantedAccess;
    reply.SaclEvaluationResults = &saclEvaluation;
    reply.Error = &error;

    if (!AuthzAccessCheck(0,
                          runtime.clientContext,
                          &request,
                          nullptr,
                          securityDescriptor,
                          nullptr,
                          0,
                          &reply,
                          nullptr)) {
        return std::nullopt;
    }

    if (error == ERROR_ACCESS_DENIED) {
        return false;
    }
    if (error != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return (grantedAccess & desiredAccess) == desiredAccess;
}

std::optional<bool> accessCheckAllowsWindows(PSECURITY_DESCRIPTOR securityDescriptor, ACCESS_MASK desiredAccess)
{
    AuthzRuntime &runtime = authzRuntime();
    if (!runtime.accessCheckReady || !runtime.impersonationToken.handle || !securityDescriptor) {
        return std::nullopt;
    }

    GENERIC_MAPPING mapping{};
    mapping.GenericRead = FILE_GENERIC_READ;
    mapping.GenericWrite = FILE_GENERIC_WRITE;
    mapping.GenericExecute = FILE_GENERIC_EXECUTE;
    mapping.GenericAll = FILE_ALL_ACCESS;

    ACCESS_MASK mappedAccess = desiredAccess;
    MapGenericMask(&mappedAccess, &mapping);

    PRIVILEGE_SET privileges{};
    DWORD privilegeLength = sizeof(privileges);
    ACCESS_MASK grantedAccess = 0;
    BOOL allowed = FALSE;
    if (!AccessCheck(securityDescriptor,
                     runtime.impersonationToken.handle,
                     mappedAccess,
                     &mapping,
                     &privileges,
                     &privilegeLength,
                     &grantedAccess,
                     &allowed)) {
        return std::nullopt;
    }

    return allowed == TRUE;
}

bool containsReadAccess(ACCESS_MASK access)
{
    constexpr ACCESS_MASK mask = FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES | READ_CONTROL;
    return (access & mask) != 0;
}

bool containsWriteAccess(ACCESS_MASK access)
{
    constexpr ACCESS_MASK mask = FILE_WRITE_DATA
        | FILE_APPEND_DATA
        | FILE_WRITE_EA
        | FILE_WRITE_ATTRIBUTES
        | FILE_DELETE_CHILD
        | DELETE
        | WRITE_DAC
        | WRITE_OWNER;
    return (access & mask) != 0;
}

bool containsExecuteAccess(ACCESS_MASK access)
{
    constexpr ACCESS_MASK mask = FILE_EXECUTE;
    return (access & mask) != 0;
}

FileAccessInfo::State mandatoryAccessStateWindows(const MandatoryLabelInfo &label, ACCESS_MASK desiredAccess)
{
    AuthzRuntime &runtime = authzRuntime();
    if (!runtime.tokenIntegrityExact || !label.exact) {
        return FileAccessInfo::State::Unknown;
    }
    if (runtime.tokenIntegrityRid >= label.integrityRid) {
        return FileAccessInfo::State::Allowed;
    }
    if ((label.policy & SYSTEM_MANDATORY_LABEL_NO_WRITE_UP) && containsWriteAccess(desiredAccess)) {
        return FileAccessInfo::State::Denied;
    }
    if ((label.policy & SYSTEM_MANDATORY_LABEL_NO_READ_UP) && containsReadAccess(desiredAccess)) {
        return FileAccessInfo::State::Denied;
    }
    if ((label.policy & SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP) && containsExecuteAccess(desiredAccess)) {
        return FileAccessInfo::State::Denied;
    }
    return FileAccessInfo::State::Allowed;
}

std::optional<bool> aclAllowsWindows(PSECURITY_DESCRIPTOR securityDescriptor, ACCESS_MASK desiredAccess)
{
    const std::optional<bool> authzAllowed = authzAllowsWindows(securityDescriptor, desiredAccess);
    if (authzAllowed.value_or(false)) {
        return true;
    }

    const std::optional<bool> accessCheckAllowed = accessCheckAllowsWindows(securityDescriptor, desiredAccess);
    if (accessCheckAllowed.has_value()) {
        return accessCheckAllowed;
    }

    return authzAllowed;
}

FileAccessInfo::State openProbeAccessStateWindows(const QString &path, ACCESS_MASK desiredAccess, bool isDirectory)
{
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    const DWORD flags = isDirectory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;
    ScopedHandle handle;
    handle.handle = CreateFileW(nativePath.c_str(),
                                desiredAccess,
                                shareMode,
                                nullptr,
                                OPEN_EXISTING,
                                flags,
                                nullptr);
    if (handle.handle != INVALID_HANDLE_VALUE) {
        return FileAccessInfo::State::Allowed;
    }

    switch (GetLastError()) {
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
        return FileAccessInfo::State::Denied;
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    default:
        return FileAccessInfo::State::Unknown;
    }
}

FileAccessInfo::State reconcileWithOpenProbeWindows(const QString &path,
                                                    ACCESS_MASK desiredAccess,
                                                    bool isDirectory,
                                                    FileAccessInfo::State resolvedState)
{
    const FileAccessInfo::State probeState = openProbeAccessStateWindows(path, desiredAccess, isDirectory);
    if (probeState == FileAccessInfo::State::Allowed || probeState == FileAccessInfo::State::Denied) {
        return probeState;
    }
    return resolvedState;
}

FileAccessInfo::State pathAccessStateWindows(const QString &path, ACCESS_MASK desiredAccess)
{
    ScopedSecurityDescriptor securityDescriptor;
    if (!loadSecurityDescriptorWindows(path, &securityDescriptor)) {
        return openProbeAccessStateWindows(path, desiredAccess, QFileInfo(path).isDir());
    }
    const FileAccessInfo::State daclState = accessStateFromOptional(
        aclAllowsWindows(securityDescriptor.descriptor, desiredAccess));
    if (daclState == FileAccessInfo::State::Denied) {
        return reconcileWithOpenProbeWindows(path,
                                             desiredAccess,
                                             QFileInfo(path).isDir(),
                                             FileAccessInfo::State::Denied);
    }

    const FileAccessInfo::State mandatoryState = mandatoryAccessStateWindows(
        mandatoryLabelWindows(path),
        desiredAccess);
    if (mandatoryState == FileAccessInfo::State::Denied) {
        return reconcileWithOpenProbeWindows(path,
                                             desiredAccess,
                                             QFileInfo(path).isDir(),
                                             FileAccessInfo::State::Denied);
    }
    if (daclState == FileAccessInfo::State::Unknown
        || mandatoryState == FileAccessInfo::State::Unknown) {
        return reconcileWithOpenProbeWindows(path,
                                             desiredAccess,
                                             QFileInfo(path).isDir(),
                                             FileAccessInfo::State::Unknown);
    }
    return reconcileWithOpenProbeWindows(path,
                                         desiredAccess,
                                         QFileInfo(path).isDir(),
                                         FileAccessInfo::State::Allowed);
}

FileAccessInfo::State parentDeleteChildAccessWindows(const QString &path)
{
    const QFileInfo info(path);
    const QString parentPath = info.absolutePath();
    if (parentPath.isEmpty() || cacheKeyForPath(parentPath) == cacheKeyForPath(path)) {
        return FileAccessInfo::State::Unknown;
    }

    return pathAccessStateWindows(parentPath, FILE_DELETE_CHILD);
}

void resolveLocalWithOpenProbeWindows(FileCapabilityInfo *result)
{
    if (!result) {
        return;
    }

    const QString path = result->path;
    auto allows = [&](ACCESS_MASK desiredAccess) {
        return openProbeAccessStateWindows(path, desiredAccess, result->isDirectory);
    };
    auto canDelete = [&]() {
        const FileAccessInfo::State targetDelete = allows(DELETE);
        if (isAllowed(targetDelete)) {
            return FileAccessInfo::State::Allowed;
        }
        const FileAccessInfo::State parentDeleteChild = parentDeleteChildAccessWindows(path);
        if (parentDeleteChild == FileAccessInfo::State::Allowed) {
            return FileAccessInfo::State::Allowed;
        }
        if (targetDelete == FileAccessInfo::State::Denied
            && parentDeleteChild == FileAccessInfo::State::Denied) {
            return FileAccessInfo::State::Denied;
        }
        return FileAccessInfo::State::Unknown;
    };

    if (result->isDirectory) {
        result->access.browseState = allows(FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES);
        result->access.createChildrenState = allows(FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY);
        result->access.traverseState = allows(FILE_TRAVERSE);
        result->access.deleteState = canDelete();
        result->access.changeAttributesState = allows(FILE_WRITE_ATTRIBUTES);
        result->access.canBrowse = isAllowed(result->access.browseState);
        result->access.canCreateChildren = isAllowed(result->access.createChildrenState);
        result->access.canTraverse = isAllowed(result->access.traverseState);
        result->access.canDelete = isAllowed(result->access.deleteState);
        result->access.canChangeAttributes = isAllowed(result->access.changeAttributesState);
        result->access.readState = result->access.browseState;
        result->access.modifyState = result->access.createChildrenState;
        result->access.executeState = result->access.traverseState;
        result->access.canRead = result->access.canBrowse;
        result->access.canModify = result->access.canCreateChildren;
        result->access.canExecute = result->access.canTraverse;
    } else {
        result->access.readState = allows(FILE_READ_DATA | FILE_READ_ATTRIBUTES);
        result->access.modifyState = allows(FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES);
        result->access.executeState = allows(FILE_EXECUTE);
        result->access.deleteState = canDelete();
        result->access.changeAttributesState = allows(FILE_WRITE_ATTRIBUTES);
        result->access.canRead = isAllowed(result->access.readState);
        result->access.canModify = isAllowed(result->access.modifyState);
        result->access.canExecute = isAllowed(result->access.executeState);
        result->access.canDelete = isAllowed(result->access.deleteState);
        result->access.canChangeAttributes = isAllowed(result->access.changeAttributesState);
        result->access.canBrowse = false;
        result->access.canCreateChildren = false;
        result->access.canTraverse = false;
    }
    result->access.exact = !hasUnknownAccessState(*result);
}

FileCapabilityInfo resolveLocalWindows(const QString &path, const QFileInfo &info)
{
    const DWORD attributeValue = windowsAttributesValue(path);
    FileCapabilityInfo result;
    result.path = path;
    result.exists = attributeValue != INVALID_FILE_ATTRIBUTES;
    result.isDirectory = result.exists
        ? ((attributeValue & FILE_ATTRIBUTE_DIRECTORY) != 0)
        : info.isDir();
    result.isArchiveLike = false;
    result.attributes = readWindowsAttributes(path, info);

    if (!result.exists) {
        return result;
    }

    ScopedSecurityDescriptor securityDescriptor;
    if (!loadSecurityDescriptorWindows(path, &securityDescriptor)) {
        resolveLocalWithOpenProbeWindows(&result);
        result.accessSummary = formatAccessSummary(result);
        result.attributesSummary = formatAttributesSummary(result);
        return result;
    }

    const MandatoryLabelInfo mandatoryLabel = mandatoryLabelWindows(path);
    auto allows = [&](ACCESS_MASK desiredAccess) {
        const FileAccessInfo::State daclState = accessStateFromOptional(
            aclAllowsWindows(securityDescriptor.descriptor, desiredAccess));
        if (daclState == FileAccessInfo::State::Denied) {
            return reconcileWithOpenProbeWindows(path, desiredAccess, result.isDirectory, FileAccessInfo::State::Denied);
        }

        const FileAccessInfo::State mandatoryState = mandatoryAccessStateWindows(mandatoryLabel, desiredAccess);
        if (mandatoryState == FileAccessInfo::State::Denied) {
            return reconcileWithOpenProbeWindows(path, desiredAccess, result.isDirectory, FileAccessInfo::State::Denied);
        }
        if (daclState == FileAccessInfo::State::Unknown
            || mandatoryState == FileAccessInfo::State::Unknown) {
            return reconcileWithOpenProbeWindows(path, desiredAccess, result.isDirectory, FileAccessInfo::State::Unknown);
        }
        return reconcileWithOpenProbeWindows(path, desiredAccess, result.isDirectory, FileAccessInfo::State::Allowed);
    };
    auto canDelete = [&]() {
        const FileAccessInfo::State targetDelete = allows(DELETE);
        if (isAllowed(targetDelete)) {
            return FileAccessInfo::State::Allowed;
        }
        const FileAccessInfo::State parentDeleteChild = parentDeleteChildAccessWindows(path);
        if (parentDeleteChild == FileAccessInfo::State::Allowed) {
            return FileAccessInfo::State::Allowed;
        }
        if (targetDelete == FileAccessInfo::State::Denied
            && parentDeleteChild == FileAccessInfo::State::Denied) {
            return FileAccessInfo::State::Denied;
        }
        return FileAccessInfo::State::Unknown;
    };

    if (result.isDirectory) {
        result.access.browseState = allows(FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES);
        result.access.createChildrenState = allows(FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY);
        result.access.traverseState = allows(FILE_TRAVERSE);
        result.access.deleteState = canDelete();
        result.access.changeAttributesState = allows(FILE_WRITE_ATTRIBUTES);
        result.access.canBrowse = isAllowed(result.access.browseState);
        result.access.canCreateChildren = isAllowed(result.access.createChildrenState);
        result.access.canTraverse = isAllowed(result.access.traverseState);
        result.access.canDelete = isAllowed(result.access.deleteState);
        result.access.canChangeAttributes = isAllowed(result.access.changeAttributesState);
        result.access.readState = result.access.browseState;
        result.access.modifyState = result.access.createChildrenState;
        result.access.executeState = result.access.traverseState;
        result.access.canRead = result.access.canBrowse;
        result.access.canModify = result.access.canCreateChildren;
        result.access.canExecute = result.access.canTraverse;
    } else {
        result.access.readState = allows(FILE_READ_DATA | FILE_READ_ATTRIBUTES);
        result.access.modifyState = allows(FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES);
        result.access.executeState = allows(FILE_EXECUTE);
        result.access.deleteState = canDelete();
        result.access.changeAttributesState = allows(FILE_WRITE_ATTRIBUTES);
        result.access.canRead = isAllowed(result.access.readState);
        result.access.canModify = isAllowed(result.access.modifyState);
        result.access.canExecute = isAllowed(result.access.executeState);
        result.access.canDelete = isAllowed(result.access.deleteState);
        result.access.canChangeAttributes = isAllowed(result.access.changeAttributesState);
        result.access.canBrowse = false;
        result.access.canCreateChildren = false;
        result.access.canTraverse = false;
    }
    result.access.exact = !hasUnknownAccessState(result);

    result.accessSummary = formatAccessSummary(result);
    result.attributesSummary = formatAttributesSummary(result);
    return result;
}

bool updateAttributeFlag(const QString &path, DWORD flag, bool enabled, QString *error)
{
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const DWORD current = GetFileAttributesW(nativePath.c_str());
    if (current == INVALID_FILE_ATTRIBUTES) {
        if (error) {
            *error = lastWindowsErrorString(GetLastError());
        }
        return false;
    }

    DWORD updated = current;
    if (enabled) {
        updated |= flag;
    } else {
        updated &= ~flag;
    }

    if (!SetFileAttributesW(nativePath.c_str(), updated)) {
        if (error) {
            *error = lastWindowsErrorString(GetLastError());
        }
        return false;
    }
    return true;
}

FileAttributesInfo readWindowsAttributes(const QString &path, const QFileInfo &info)
{
    FileAttributesInfo attributes;
    const DWORD value = windowsAttributesValue(path);
    if (value == INVALID_FILE_ATTRIBUTES) {
        attributes.hidden = info.isHidden();
        attributes.readOnly = false;
        attributes.system = false;
        attributes.archive = false;
        return attributes;
    }

    attributes.hidden = (value & FILE_ATTRIBUTE_HIDDEN) != 0;
    attributes.readOnly = (value & FILE_ATTRIBUTE_READONLY) != 0;
    attributes.system = (value & FILE_ATTRIBUTE_SYSTEM) != 0;
    attributes.archive = (value & FILE_ATTRIBUTE_ARCHIVE) != 0;
    return attributes;
}
#endif

FileAttributesInfo readFileAttributes(const QString &path, const QFileInfo &info)
{
#ifdef Q_OS_WIN
    return readWindowsAttributes(path, info);
#else
    Q_UNUSED(path)
    FileAttributesInfo attributes;
    attributes.hidden = info.isHidden();
    attributes.readOnly = info.exists() && !info.isWritable();
    return attributes;
#endif
}

#ifndef FM_ACCESS_RESOLVER_LOCAL_ONLY
FileCapabilityInfo resolveArchivePath(const QString &path)
{
    FileCapabilityInfo result;
    result.path = path;
    result.exists = true;
    result.isArchiveLike = true;
    result.attributes.archive = true;

    const std::optional<FileEntry> entry = ArchiveFileProvider::cachedEntryInfo(path);
    if (entry) {
        result.isDirectory = entry->isDirectory;
        result.attributes.hidden = entry->isHidden;
        result.attributes.readOnly = entry->isReadOnly;
        result.attributes.system = entry->isSystem;
    } else {
        result.isDirectory = ArchiveSupport::archiveBrowsePath(path) == QLatin1String("/");
        result.attributes.hidden = false;
        result.attributes.readOnly = true;
        result.attributes.system = false;
    }

    result.access.canRead = true;
    result.access.canBrowse = result.isDirectory;
    result.access.canTraverse = result.isDirectory;
    result.access.readState = FileAccessInfo::State::Allowed;
    result.access.browseState = accessStateFromBool(result.access.canBrowse);
    result.access.traverseState = accessStateFromBool(result.access.canTraverse);
    result.access.modifyState = FileAccessInfo::State::Denied;
    result.access.deleteState = FileAccessInfo::State::Denied;
    result.access.executeState = FileAccessInfo::State::Denied;
    result.access.createChildrenState = FileAccessInfo::State::Denied;
    result.access.changeAttributesState = FileAccessInfo::State::Denied;
    result.access.exact = true;
    result.accessSummary = formatAccessSummary(result);
    result.attributesSummary = formatAttributesSummary(result);
    return result;
}
#endif

FileCapabilityInfo resolveFallback(const QString &path, const QFileInfo &info)
{
    FileCapabilityInfo result;
    result.path = path;
    result.exists = info.exists();
    result.isDirectory = info.isDir();
    result.isArchiveLike = false;
    result.attributes = readFileAttributes(path, info);
    result.access.exact = false;

    if (result.isDirectory) {
        result.access.canBrowse = info.isReadable();
        result.access.canCreateChildren = info.isWritable();
        result.access.canTraverse = info.isExecutable() || info.isReadable();
        result.access.canDelete = QFileInfo(info.absolutePath()).isWritable();
        result.access.canChangeAttributes = info.isWritable();
        result.access.browseState = accessStateFromBool(result.access.canBrowse);
        result.access.createChildrenState = accessStateFromBool(result.access.canCreateChildren);
        result.access.traverseState = accessStateFromBool(result.access.canTraverse);
        result.access.deleteState = accessStateFromBool(result.access.canDelete);
        result.access.changeAttributesState = accessStateFromBool(result.access.canChangeAttributes);
        result.access.readState = result.access.browseState;
        result.access.modifyState = result.access.createChildrenState;
        result.access.executeState = result.access.traverseState;
        result.access.canRead = result.access.canBrowse;
        result.access.canModify = result.access.canCreateChildren;
        result.access.canExecute = result.access.canTraverse;
    } else {
        result.access.canRead = info.isReadable();
        result.access.canModify = info.isWritable();
        result.access.canDelete = QFileInfo(info.absolutePath()).isWritable();
        result.access.canExecute = info.isExecutable();
        result.access.canChangeAttributes = info.isWritable();
        result.access.readState = accessStateFromBool(result.access.canRead);
        result.access.modifyState = accessStateFromBool(result.access.canModify);
        result.access.deleteState = accessStateFromBool(result.access.canDelete);
        result.access.executeState = accessStateFromBool(result.access.canExecute);
        result.access.changeAttributesState = accessStateFromBool(result.access.canChangeAttributes);
    }

    result.accessSummary = formatAccessSummary(result);
    result.attributesSummary = formatAttributesSummary(result);
    return result;
}

} // namespace

FileCapabilityInfo FileAccessResolver::resolve(const QString &path)
{
    if (path.isEmpty()) {
        return {};
    }

#ifndef FM_ACCESS_RESOLVER_LOCAL_ONLY
    if (ArchiveSupport::isArchivePath(path)) {
        return resolveArchivePath(path);
    }
#endif

    const QFileInfo info(path);
    const QString key = cacheKeyForPath(path);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
#ifdef Q_OS_WIN
    const bool cacheExists = windowsAttributesValue(path) != INVALID_FILE_ATTRIBUTES;
#else
    const bool cacheExists = info.exists();
#endif
    const qint64 infoModifiedMs = cacheExists ? lastModifiedStamp(info) : -1;
    const qint64 infoSize = cacheExists ? info.size() : -1;

    {
        QMutexLocker locker(&cacheMutex());
        const auto it = cacheStore().constFind(key);
        if (it != cacheStore().constEnd()) {
            const CacheEntry &entry = it.value();
            if (entry.exists == cacheExists
                && entry.lastModifiedMs == infoModifiedMs
                && entry.size == infoSize
                && (nowMs - entry.cachedAtMs) < 3000) {
                return entry.info;
            }
        }
    }

    FileCapabilityInfo result;
#ifdef Q_OS_WIN
    result = resolveLocalWindows(path, info);
#else
    result = resolveFallback(path, info);
#endif
    result.accessSummary = formatAccessSummary(result);
    result.attributesSummary = formatAttributesSummary(result);

    {
        QMutexLocker locker(&cacheMutex());
        if (cacheStore().size() > 256) {
            cacheStore().clear();
        }
        cacheStore().insert(key, CacheEntry{result, infoSize, infoModifiedMs, cacheExists, nowMs});
    }

    return result;
}

QVariantList FileAccessResolver::accessProperties(const FileCapabilityInfo &info)
{
    QVariantList rows;
    if (info.isDirectory) {
        rows.append(makeProperty(QStringLiteral("Browse"), info.access.browseState));
        rows.append(makeProperty(QStringLiteral("Create inside"), info.access.createChildrenState));
        rows.append(makeProperty(QStringLiteral("Delete"), info.access.deleteState));
        rows.append(makeProperty(QStringLiteral("Traverse"), info.access.traverseState));
    } else {
        rows.append(makeProperty(QStringLiteral("Read"), info.access.readState));
        rows.append(makeProperty(QStringLiteral("Modify"), info.access.modifyState));
        rows.append(makeProperty(QStringLiteral("Delete"), info.access.deleteState));
        rows.append(makeProperty(QStringLiteral("Execute"), info.access.executeState));
    }
    return rows;
}

QVariantList FileAccessResolver::attributeProperties(const FileCapabilityInfo &info)
{
    QVariantList rows;
    rows.append(makeAttributeProperty(QStringLiteral("Hidden"), info.attributes.hidden));
    rows.append(makeAttributeProperty(QStringLiteral("Read-only"), info.attributes.readOnly));
    rows.append(makeAttributeProperty(QStringLiteral("System"), info.attributes.system));
    return rows;
}

bool FileAccessResolver::setHidden(const QString &path, bool enabled, QString *error)
{
#ifdef Q_OS_WIN
    const bool ok = updateAttributeFlag(path, FILE_ATTRIBUTE_HIDDEN, enabled, error);
    if (ok) {
        invalidate(path);
    }
    return ok;
#else
    Q_UNUSED(path)
    Q_UNUSED(enabled)
    if (error) {
        *error = QStringLiteral("Hidden attribute is not supported here");
    }
    return false;
#endif
}

bool FileAccessResolver::setReadOnly(const QString &path, bool enabled, QString *error)
{
#ifdef Q_OS_WIN
    const bool ok = updateAttributeFlag(path, FILE_ATTRIBUTE_READONLY, enabled, error);
    if (ok) {
        invalidate(path);
    }
    return ok;
#else
    Q_UNUSED(path)
    Q_UNUSED(enabled)
    if (error) {
        *error = QStringLiteral("Read-only attribute is not supported here");
    }
    return false;
#endif
}

void FileAccessResolver::invalidate(const QString &path)
{
    QMutexLocker locker(&cacheMutex());
    cacheStore().remove(cacheKeyForPath(path));
}

void FileAccessResolver::invalidateAll()
{
    QMutexLocker locker(&cacheMutex());
    cacheStore().clear();
}
