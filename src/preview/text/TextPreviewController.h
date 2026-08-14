#pragma once

#include "TextPreviewTypes.h"

#include <QObject>
#include <QThreadPool>
#include <QVariantMap>
#include <QVector>

#include <functional>

class TextPreviewController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshotMap NOTIFY snapshotChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(int totalLineCount READ totalLineCount NOTIFY totalLineCountChanged)

public:
    using PageReader = std::function<TextPreview::Snapshot(
        qint64, qint64, TextPreview::Encoding, const TextPreview::ReadOptions &)>;
    using LineCounter = std::function<int()>;
    using Decorator = std::function<void(TextPreview::Snapshot &, const QByteArray &)>;

    explicit TextPreviewController(QObject *parent = nullptr);
    ~TextPreviewController() override;

    QVariantMap snapshotMap() const;
    bool loading() const;
    const TextPreview::Snapshot &snapshot() const;
    QString path() const;
    int pageIndex() const;
    int totalLineCount() const;

    void setReadOptions(const TextPreview::ReadOptions &options);
    Q_INVOKABLE void loadLocalFile(const QString &path);
    void loadSource(const QString &identity, PageReader reader, LineCounter lineCounter = {},
                    Decorator decorator = {});
    void loadLocalFile(const QString &path, Decorator decorator);
    Q_INVOKABLE void requestPreviousPage();
    Q_INVOKABLE void requestNextPage();
    Q_INVOKABLE void cancel();

signals:
    void snapshotChanged();
    void loadingChanged();
    void totalLineCountChanged();

private:
    struct PageEntry {
        TextPreview::Snapshot snapshot;
        bool cached = false;
    };

    void requestPage(qint64 byteOffset, qint64 firstLine, TextPreview::Encoding encoding,
                     bool appendPage, int targetPageIndex = -1,
                     const QByteArray &decorationInitialState = {});
    void publishPage(TextPreview::Snapshot snapshot, bool appendPage, int targetPageIndex,
                     quint64 generation, quint64 requestRevision);
    void showPage(int pageIndex);
    void trimPageCache();
    void setLoading(bool loading);

    QThreadPool m_taskPool;
    QThreadPool m_countPool;
    QString m_path;
    PageReader m_pageReader;
    Decorator m_decorator;
    TextPreview::ReadOptions m_options;
    TextPreview::Snapshot m_snapshot;
    QVector<PageEntry> m_pages;
    int m_pageIndex = -1;
    int m_totalLineCount = 0;
    quint64 m_generation = 0;
    quint64 m_requestRevision = 0;
    bool m_loading = false;
};
