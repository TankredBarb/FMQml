QString LocalFileProvider::scheme() const
{
    return QStringLiteral("file");
}

bool LocalFileProvider::canHandle(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    return info.isAbsolute()
        || path.startsWith('/')
        || path.startsWith(QStringLiteral("\\\\"))
        || path.startsWith(QStringLiteral("file:"));
}

LocalFileProvider::Capabilities LocalFileProvider::capabilities() const
{
    return Browse
        | ReadMetadata
        | Create
        | Rename
        | Remove
        | Transfer
        | Watch;
}

bool LocalFileProvider::pathExists(const QString &path) const
{
    QElapsedTimer timer;
    timer.start();
#ifdef Q_OS_WIN
    const bool result = fileAttributesWindows(path) != INVALID_FILE_ATTRIBUTES;
#else
    const bool result = QFileInfo::exists(path)
#ifdef Q_OS_LINUX
        || (!LinuxAdminBroker::activeSessionNonce().isEmpty() && entryInfo(path).has_value())
#endif
        ;
#endif
    traceLocalProviderSlow("pathExists", path, timer.elapsed(),
                           QStringLiteral("result=%1").arg(result));
    return result;
}

bool LocalFileProvider::isDirectory(const QString &path) const
{
    QElapsedTimer timer;
    timer.start();
#ifdef Q_OS_WIN
    const DWORD attributes = fileAttributesWindows(path);
    const bool result = attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    const QFileInfo info(path);
    bool result = info.isDir();
#ifdef Q_OS_LINUX
    if (!result && !info.exists() && !LinuxAdminBroker::activeSessionNonce().isEmpty()) {
        const std::optional<FileEntry> entry = entryInfo(path);
        result = entry && entry->isDirectory;
    }
#endif
#endif
    traceLocalProviderSlow("isDirectory", path, timer.elapsed(),
                           QStringLiteral("result=%1").arg(result));
    return result;
}

bool LocalFileProvider::isSymLink(const QString &path) const
{
    const QFileInfo info(path);
    if (info.exists() || info.isSymLink()) {
        return info.isSymLink();
    }
#ifdef Q_OS_LINUX
    if (!LinuxAdminBroker::activeSessionNonce().isEmpty()) {
        const std::optional<FileEntry> entry = entryInfo(path);
        return entry && entry->isSymLink;
    }
#endif
    return false;
}

QString LocalFileProvider::normalizedPath(const QString &path) const
{
    return normalizedFilesystemPath(path);
}

QString LocalFileProvider::fileName(const QString &path) const
{
    return QFileInfo(path).fileName();
}

QString LocalFileProvider::absolutePath(const QString &path) const
{
    return QFileInfo(path).absoluteFilePath();
}

QString LocalFileProvider::parentPath(const QString &path) const
{
    return QFileInfo(path).absoluteDir().absolutePath();
}

QString LocalFileProvider::childPath(const QString &parentPath, const QString &name) const
{
    return QDir(parentPath).filePath(name);
}

std::optional<FileEntry> LocalFileProvider::entryInfo(const QString &path) const
{
    QFileInfo info(path);
#ifdef Q_OS_WIN
    if (!info.exists() && entryAttributesWindows(info) == INVALID_FILE_ATTRIBUTES) {
        return std::nullopt;
    }
#else
    if (!info.exists()) {
#ifdef Q_OS_LINUX
        const QString nonce = LinuxAdminBroker::activeSessionNonce();
        if (!nonce.isEmpty()) {
            LinuxAdminBroker::Request request;
            request.operation = LinuxAdminBroker::Operation::ListDirectory;
            request.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            request.sessionNonce = nonce;
            request.sourcePath = QFileInfo(path).absolutePath();
            request.includeHidden = true;
            LinuxAdminBroker broker;
            const LinuxAdminBroker::Result result = broker.submitBlocking(request);
            if (result.success) {
                const QString wanted = normalizedFilesystemPath(path);
                const QList<FileEntry> entries = entriesFromAdminHelper(result.entries, QLocale{});
                for (const FileEntry &entry : entries) {
                    if (sameFilesystemPath(entry.path, wanted)) {
                        return entry;
                    }
                }
            }
        }
#endif
        return std::nullopt;
    }
#endif
    return entryFromInfo(info);
}

bool LocalFileProvider::ensureParentDirectory(const QString &path) const
{
    clearLastError();
    const QString parentPath = QFileInfo(path).absolutePath();
    if (QDir().mkpath(parentPath)) {
        return true;
    }
    setLastError(QStringLiteral("Cannot create folder in %1")
                     .arg(QDir::toNativeSeparators(parentPath)));
    return false;
}

bool LocalFileProvider::makePath(const QString &path) const
{
    clearLastError();
    if (QDir().mkpath(path)) {
        return true;
    }
    setLastError(QStringLiteral("Cannot create folder in %1")
                     .arg(QDir::toNativeSeparators(QFileInfo(path).absolutePath())));
    return false;
}

bool LocalFileProvider::removePath(const QString &path) const
{
    clearLastError();
    QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }

#ifdef Q_OS_WIN
    std::error_code errorCode;
    const std::filesystem::path nativePath = std::filesystem::path(QDir::toNativeSeparators(path).toStdWString());
    if (info.isDir() && !info.isSymLink()) {
        std::filesystem::remove_all(nativePath, errorCode);
        if (errorCode) {
            setLastError(windowsMutationErrorMessage(FileMutationKind::DeleteFolder, path, static_cast<DWORD>(errorCode.value())));
            return false;
        }
        return true;
    }

    const bool removed = std::filesystem::remove(nativePath, errorCode);
    if (errorCode) {
        setLastError(windowsMutationErrorMessage(FileMutationKind::DeleteFile, path, static_cast<DWORD>(errorCode.value())));
        return false;
    }
    return removed;
#else
    if (info.isDir() && !info.isSymLink()) {
        const bool removed = QDir(path).removeRecursively();
        return removed;
    }
    return QFile::remove(path);
#endif
}

QStringList LocalFileProvider::childPaths(const QString &path, bool includeHidden) const
{
    QElapsedTimer timer;
    timer.start();
    QStringList children;

#ifdef Q_OS_LINUX
    QList<LinuxFileEnumerator::Entry> entries;
    QString error;
    LinuxFileEnumerator::Options options;
    options.includeHidden = includeHidden;
    if (!LinuxFileEnumerator::enumerateChildren(path, options, &entries, &error)) {
        const QString nonce = LinuxAdminBroker::activeSessionNonce();
        if (!nonce.isEmpty()) {
            LinuxAdminBroker::Request request;
            request.operation = LinuxAdminBroker::Operation::ListDirectory;
            request.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            request.sessionNonce = nonce;
            request.sourcePath = path;
            request.includeHidden = includeHidden;
            LinuxAdminBroker broker;
            const LinuxAdminBroker::Result result = broker.submitBlocking(request);
            if (result.success) {
                const QList<FileEntry> adminEntries = entriesFromAdminHelper(result.entries, QLocale{});
                children.reserve(adminEntries.size());
                for (const FileEntry &entry : adminEntries) {
                    children.append(entry.path);
                }
                traceLocalProviderNav("childPaths", path,
                                      QStringLiteral("count=%1 includeHidden=%2 admin=1 elapsedMs=%3")
                                          .arg(children.size())
                                          .arg(includeHidden)
                                          .arg(timer.elapsed()));
                return children;
            }
        }
        traceLocalProviderNav("childPaths", path,
                              QStringLiteral("result=opendir-failed includeHidden=%1 elapsedMs=%2 error=%3")
                                  .arg(includeHidden)
                                  .arg(timer.elapsed())
                                  .arg(error));
        return children;
    }

    children.reserve(entries.size());
    for (const LinuxFileEnumerator::Entry &entry : std::as_const(entries)) {
        children.append(entry.path);
    }

    traceLocalProviderNav("childPaths", path,
                          QStringLiteral("count=%1 includeHidden=%2 elapsedMs=%3 native=1")
                              .arg(children.size())
                              .arg(includeHidden)
                              .arg(timer.elapsed()));
    return children;
#endif

    QDir dir(path);
    const QFileInfoList infos = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | (includeHidden ? QDir::Hidden : QDir::NoFilter));
    children.reserve(infos.size());
    for (const QFileInfo &info : infos) {
        const bool isHidden = info.isHidden() || info.fileName().startsWith('.');
        if (!includeHidden && isHidden) {
            continue;
        }
        children.append(info.absoluteFilePath());
    }
    traceLocalProviderNav("childPaths", path,
                          QStringLiteral("count=%1 includeHidden=%2 elapsedMs=%3")
                              .arg(children.size())
                              .arg(includeHidden)
                              .arg(timer.elapsed()));
    return children;
}

BoundedFolderPreviewResult LocalFileProvider::boundedFolderPreview(
    const QString &path, bool includeHidden, int maxEntries,
    const std::function<bool()> &shouldCancel) const
{
    BoundedFolderPreviewResult preview;
    if (maxEntries <= 0 || (shouldCancel && shouldCancel())) {
        return preview;
    }

#ifdef Q_OS_LINUX
    const QString nonce = LinuxAdminBroker::activeSessionNonce();
    if (!nonce.isEmpty()) {
        LinuxAdminBroker::Request request;
        request.operation = LinuxAdminBroker::Operation::ListDirectory;
        request.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        request.sessionNonce = nonce;
        request.sourcePath = path;
        request.includeHidden = includeHidden;
        request.length = qint64(maxEntries) + 1;

        LinuxAdminBroker broker;
        const LinuxAdminBroker::Result result = broker.submitBlocking(request);
        if (!result.success || (shouldCancel && shouldCancel())) {
            return preview;
        }

        QList<FileEntry> entries = entriesFromAdminHelper(result.entries, QLocale{});
        entries.removeIf([](const FileEntry &entry) {
            return entry.name == QLatin1String(".") || entry.name == QLatin1String("..");
        });
        preview.hasMore = entries.size() > maxEntries;
        if (preview.hasMore) {
            entries.resize(maxEntries);
        }
        preview.entries = std::move(entries);
        preview.status = BoundedFolderPreviewResult::Status::Ready;
    }
#else
    Q_UNUSED(path)
    Q_UNUSED(includeHidden)
#endif
    return preview;
}
