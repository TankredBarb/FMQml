#include "AudioTagEditorSession.h"

#include <QCoreApplication>

#include <cstdio>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

QVariantMap item(const QString &path)
{
    return {{QStringLiteral("path"), path}, {QStringLiteral("name"), path.section('/', -1)},
            {QStringLiteral("ok"), true}, {QStringLiteral("title"), QStringLiteral("Track")},
            {QStringLiteral("artist"), QString()}, {QStringLiteral("album"), QString()},
            {QStringLiteral("lyrics"), QString()}, {QStringLiteral("coverWriteSupported"), true}};
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    AudioTagEditorSession session;
    session.editModel()->setRecords({item(QStringLiteral("/tmp/a.mp3")),
                                     item(QStringLiteral("/tmp/b.mp3"))});
    session.setCurrentIndex(0);

    int lyricsAppliedCount = 0;
    QObject::connect(&session, &AudioTagEditorSession::lyricsApplied,
                     [&lyricsAppliedCount]() { ++lyricsAppliedCount; });
    if (!expect(session.useLyricsCandidate({{QStringLiteral("plainLyrics"), QStringLiteral("Words")}}),
                "A lyrics candidate must update the current record")) return 1;
    if (!expect(session.currentRecord().value(QStringLiteral("lyrics")).toString() == QStringLiteral("Words")
                    && session.dirtyCount() == 1 && lyricsAppliedCount == 1,
                "Lyrics selection must be owned and signalled by the session")) return 1;

    session.fetchCoverCandidates();
    if (!expect(!session.coverLookupBusy() && session.coverLookupStatusIsError()
                    && !session.coverLookupStatus().isEmpty(),
                "Synchronous lookup validation failure must settle session state")) return 1;
    session.setCurrentIndex(1);
    if (!expect(session.coverLookupStatus().isEmpty() && !session.coverLookupStatusIsError()
                    && !session.lookupBusy(),
                "Changing rows must cancel and clear lookup state")) return 1;
    return 0;
}
