#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

class QNetworkAccessManager;
class QNetworkReply;

class AudioTagEditorBackend : public QObject
{
    Q_OBJECT

public:
    explicit AudioTagEditorBackend(QObject *parent = nullptr);
    ~AudioTagEditorBackend() override;

    Q_INVOKABLE QString localPathFromUrl(const QString &value) const;
    Q_INVOKABLE QVariantList loadTags(const QStringList &paths) const;
    Q_INVOKABLE QVariantMap applyChanges(const QVariantList &items) const;

    Q_INVOKABLE void lookupCoverArtAsync(const QVariantMap &item, int requestId);
    Q_INVOKABLE void downloadCoverArtAsync(const QString &imageUrl, int requestId);
    Q_INVOKABLE void cancelCoverLookups();
    Q_INVOKABLE void releaseCoverStaging();

    Q_INVOKABLE void lookupLyricsAsync(const QVariantMap &item, int requestId);
    Q_INVOKABLE void cancelLyricsLookup();

    Q_INVOKABLE void lookupTagsAsync(const QVariantMap &item, int requestId);
    Q_INVOKABLE void cancelTagsLookup();

signals:
    void coverLookupFinished(int requestId, const QVariantMap &result);
    void coverDownloadFinished(int requestId, const QVariantMap &result);
    void lyricsLookupFinished(int requestId, const QVariantMap &result);
    void tagsLookupFinished(int requestId, const QVariantMap &result);

private:
    using ReplyList = std::vector<QPointer<QNetworkReply>>;

    void abortReplies(ReplyList &replies);
    void dropReply(ReplyList &replies, QNetworkReply *reply);
    void registerCoverReply(QNetworkReply *reply);
    void registerLyricsReply(QNetworkReply *reply);
    void registerTagReply(QNetworkReply *reply);
    void registerCoverDownloadReply(QNetworkReply *reply);
    QString ensureCoverStagingDir();

    QNetworkAccessManager *m_network = nullptr;
    ReplyList m_coverReplies;
    ReplyList m_lyricsReplies;
    ReplyList m_tagReplies;
    QPointer<QNetworkReply> m_coverDownloadReply;
    QString m_coverStagingDir;
    QString m_coverStagingLeaseId;
};
