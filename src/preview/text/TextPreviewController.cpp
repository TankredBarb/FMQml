#include "TextPreviewController.h"

#include "TextPreviewReader.h"

#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

namespace {
QString stateName(TextPreview::State state)
{
    switch (state) {
    case TextPreview::State::Empty: return QStringLiteral("empty");
    case TextPreview::State::Loading: return QStringLiteral("loading");
    case TextPreview::State::Ready: return QStringLiteral("ready");
    case TextPreview::State::NotText: return QStringLiteral("notText");
    case TextPreview::State::ReadError: return QStringLiteral("readError");
    case TextPreview::State::DecodeError: return QStringLiteral("decodeError");
    }
    return QStringLiteral("readError");
}

QString modeName(TextPreview::Mode mode)
{
    return mode == TextPreview::Mode::Complete ? QStringLiteral("complete")
                                               : QStringLiteral("windowed");
}

QString kindName(TextPreview::Kind kind)
{
    switch (kind) {
    case TextPreview::Kind::Plain: return QStringLiteral("plain");
    case TextPreview::Kind::Code: return QStringLiteral("code");
    case TextPreview::Kind::Script: return QStringLiteral("script");
    case TextPreview::Kind::Structured: return QStringLiteral("structured");
    case TextPreview::Kind::Log: return QStringLiteral("log");
    }
    return QStringLiteral("plain");
}
}

TextPreviewController::TextPreviewController(QObject *parent)
    : QObject(parent)
{
    m_taskPool.setMaxThreadCount(1);
    m_taskPool.setExpiryTimeout(30000);
    m_countPool.setMaxThreadCount(1);
    m_countPool.setExpiryTimeout(30000);
}

TextPreviewController::~TextPreviewController()
{
    ++m_generation;
    ++m_requestRevision;
    m_taskPool.clear();
    m_taskPool.waitForDone();
    m_countPool.clear();
    m_countPool.waitForDone();
}

QVariantMap TextPreviewController::snapshotMap() const
{
    QVariantMap result;
    result.insert(QStringLiteral("state"), stateName(m_snapshot.state));
    result.insert(QStringLiteral("mode"), modeName(m_snapshot.mode));
    result.insert(QStringLiteral("kind"), kindName(m_snapshot.classification.kind));
    result.insert(QStringLiteral("content"), m_snapshot.content);
    result.insert(QStringLiteral("languageId"), m_snapshot.classification.languageId);
    result.insert(QStringLiteral("languageLabel"), m_snapshot.classification.languageLabel);
    result.insert(QStringLiteral("defaultWrap"), m_snapshot.classification.defaultWrap);
    result.insert(QStringLiteral("defaultLineNumbers"), m_snapshot.classification.defaultLineNumbers);
    result.insert(QStringLiteral("encodingLabel"), m_snapshot.encodingLabel);
    result.insert(QStringLiteral("errorText"), m_snapshot.errorText);
    result.insert(QStringLiteral("totalBytes"), m_snapshot.totalBytes);
    result.insert(QStringLiteral("byteOffset"), m_snapshot.byteOffset);
    result.insert(QStringLiteral("byteLength"), m_snapshot.byteLength);
    result.insert(QStringLiteral("firstLine"), m_snapshot.firstLine);
    result.insert(QStringLiteral("visibleLineCount"), m_snapshot.visibleLineCount);
    result.insert(QStringLiteral("lineAdvance"), m_snapshot.lineAdvance);
    result.insert(QStringLiteral("hasPreviousPage"), m_pageIndex > 0);
    result.insert(QStringLiteral("hasNextPage"), m_snapshot.hasNextPage);
    result.insert(QStringLiteral("generation"), QVariant::fromValue(m_generation));
    result.insert(QStringLiteral("requestRevision"), QVariant::fromValue(m_requestRevision));
    return result;
}

bool TextPreviewController::loading() const
{
    return m_loading;
}

const TextPreview::Snapshot &TextPreviewController::snapshot() const
{
    return m_snapshot;
}

QString TextPreviewController::path() const
{
    return m_path;
}

int TextPreviewController::pageIndex() const
{
    return m_pageIndex;
}

int TextPreviewController::totalLineCount() const
{
    return m_totalLineCount;
}

void TextPreviewController::setReadOptions(const TextPreview::ReadOptions &options)
{
    m_options = options;
}

void TextPreviewController::loadLocalFile(const QString &path)
{
    loadLocalFile(path, {});
}

void TextPreviewController::loadLocalFile(const QString &path, Decorator decorator)
{
    loadSource(path, [path](qint64 byteOffset, qint64 firstLine,
                            TextPreview::Encoding encoding,
                            const TextPreview::ReadOptions &options) {
        return TextPreview::readFilePage(path, byteOffset, firstLine, encoding, options);
    }, [path]() { return TextPreview::countFileLines(path); }, std::move(decorator));
}

void TextPreviewController::loadSource(const QString &identity, PageReader reader,
                                       LineCounter lineCounter, Decorator decorator)
{
    ++m_generation;
    ++m_requestRevision;
    m_taskPool.clear();
    m_countPool.clear();
    m_path = identity;
    m_pageReader = std::move(reader);
    m_decorator = std::move(decorator);
    m_pages.clear();
    m_pageIndex = -1;
    m_totalLineCount = 0;
    m_snapshot = {};
    m_snapshot.state = TextPreview::State::Loading;
    emit snapshotChanged();
    emit totalLineCountChanged();
    requestPage(0, 1, TextPreview::Encoding::Auto, true, -1, {});
    if (lineCounter) {
        const quint64 generation = m_generation;
        QPointer<TextPreviewController> self(this);
        (void)QtConcurrent::run(&m_countPool, [self, generation,
                                               lineCounter = std::move(lineCounter)]() {
            const int lineCount = lineCounter();
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(self.data(), [self, generation, lineCount]() {
                if (!self || generation != self->m_generation) {
                    return;
                }
                self->m_totalLineCount = lineCount;
                emit self->totalLineCountChanged();
            }, Qt::QueuedConnection);
        });
    }
}

void TextPreviewController::requestPreviousPage()
{
    if (m_loading || m_pageIndex <= 0) {
        return;
    }
    const int targetIndex = m_pageIndex - 1;
    if (m_pages.at(targetIndex).cached) {
        ++m_requestRevision;
        showPage(targetIndex);
        return;
    }
    const TextPreview::Snapshot &anchor = m_pages.at(targetIndex).snapshot;
    requestPage(anchor.byteOffset, anchor.firstLine, anchor.encoding, false, targetIndex,
                anchor.decorationInitialState);
}

void TextPreviewController::requestNextPage()
{
    if (m_loading || m_pageIndex < 0 || !m_snapshot.hasNextPage) {
        return;
    }
    if (m_pageIndex + 1 < m_pages.size()) {
        const int targetIndex = m_pageIndex + 1;
        if (m_pages.at(targetIndex).cached) {
            ++m_requestRevision;
            showPage(targetIndex);
            return;
        }
        const TextPreview::Snapshot &anchor = m_pages.at(targetIndex).snapshot;
        requestPage(anchor.byteOffset, anchor.firstLine, anchor.encoding, false, targetIndex,
                    anchor.decorationInitialState);
        return;
    }
    requestPage(m_snapshot.byteOffset + m_snapshot.byteLength,
                m_snapshot.firstLine + m_snapshot.lineAdvance,
                m_snapshot.encoding, true, -1, m_snapshot.decorationFinalState);
}

void TextPreviewController::cancel()
{
    ++m_generation;
    ++m_requestRevision;
    m_taskPool.clear();
    m_countPool.clear();
    m_path.clear();
    m_pageReader = {};
    m_decorator = {};
    m_pages.clear();
    m_pageIndex = -1;
    m_totalLineCount = 0;
    m_snapshot = {};
    setLoading(false);
    emit snapshotChanged();
    emit totalLineCountChanged();
}

void TextPreviewController::requestPage(qint64 byteOffset, qint64 firstLine,
                                        TextPreview::Encoding encoding, bool appendPage,
                                        int targetPageIndex,
                                        const QByteArray &decorationInitialState)
{
    if (m_path.isEmpty() || !m_pageReader) {
        return;
    }
    const PageReader pageReader = m_pageReader;
    const Decorator decorator = m_decorator;
    const TextPreview::ReadOptions options = m_options;
    const quint64 generation = m_generation;
    const quint64 requestRevision = ++m_requestRevision;
    setLoading(true);

    QPointer<TextPreviewController> self(this);
    (void)QtConcurrent::run(&m_taskPool, [self, pageReader, decorator, byteOffset, firstLine, encoding,
                                         options, appendPage, targetPageIndex,
                                         generation, requestRevision, decorationInitialState]() mutable {
        TextPreview::Snapshot snapshot = pageReader(byteOffset, firstLine, encoding, options);
        snapshot.decorationInitialState = decorationInitialState;
        if (snapshot.state == TextPreview::State::Ready && decorator) {
            decorator(snapshot, decorationInitialState);
        }
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(),
            [self, snapshot = std::move(snapshot), appendPage, targetPageIndex,
             generation, requestRevision]() mutable {
                if (!self) {
                    return;
                }
                self->publishPage(std::move(snapshot), appendPage, targetPageIndex,
                                  generation, requestRevision);
            }, Qt::QueuedConnection);
    });
}

void TextPreviewController::publishPage(TextPreview::Snapshot snapshot, bool appendPage,
                                        int targetPageIndex,
                                        quint64 generation, quint64 requestRevision)
{
    if (generation != m_generation || requestRevision != m_requestRevision) {
        return;
    }
    if (appendPage) {
        if (m_pageIndex + 1 < m_pages.size()) {
            m_pages.resize(m_pageIndex + 1);
        }
        m_pages.append(PageEntry{snapshot, true});
        m_pageIndex = m_pages.size() - 1;
    } else if (targetPageIndex >= 0 && targetPageIndex < m_pages.size()) {
        m_pages[targetPageIndex] = PageEntry{snapshot, true};
        m_pageIndex = targetPageIndex;
    }
    m_snapshot = std::move(snapshot);
    trimPageCache();
    setLoading(false);
    emit snapshotChanged();
}

void TextPreviewController::showPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_pages.size() || !m_pages.at(pageIndex).cached) {
        return;
    }
    m_pageIndex = pageIndex;
    m_snapshot = m_pages.at(pageIndex).snapshot;
    trimPageCache();
    emit snapshotChanged();
}

void TextPreviewController::trimPageCache()
{
    for (int index = 0; index < m_pages.size(); ++index) {
        if (qAbs(index - m_pageIndex) <= 1 || !m_pages.at(index).cached) {
            continue;
        }
        m_pages[index].snapshot.content.clear();
        m_pages[index].cached = false;
    }
}

void TextPreviewController::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}
