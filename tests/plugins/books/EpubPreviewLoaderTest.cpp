#include "EpubPreviewLoader.h"

#include <QCoreApplication>

#include <cstdio>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QByteArray container = R"(<?xml version="1.0"?>
        <container><rootfiles><rootfile full-path="OPS/content.opf"/></rootfiles></container>)";
    const QByteArray package = R"(<?xml version="1.0"?>
        <package>
          <metadata>
            <dc:title xmlns:dc="urn:test">  Test book  </dc:title>
            <dc:creator xmlns:dc="urn:test">First Author</dc:creator>
            <dc:creator xmlns:dc="urn:test">Second Author</dc:creator>
            <dc:subject xmlns:dc="urn:test">Fiction</dc:subject>
            <dc:date xmlns:dc="urn:test">2026</dc:date>
            <dc:language xmlns:dc="urn:test">uk</dc:language>
            <dc:description xmlns:dc="urn:test">A short description</dc:description>
            <meta name="cover" content="legacy-cover"/>
          </metadata>
          <manifest>
            <item id="chapter-1" href="text/chapter-1.xhtml"/>
            <item id="chapter-2" href="text/chapter-2.xhtml"/>
            <item id="legacy-cover" href="images/legacy.jpg"/>
            <item id="epub3-cover" href="images/cover.jpg" properties="nav cover-image"/>
          </manifest>
          <spine><itemref idref="chapter-1"/><itemref idref="chapter-2"/></spine>
        </package>)";

    const PreviewInternal::EpubPackageData data = PreviewInternal::parseEpubPackageData(container, package);
    bool ok = expect(data.error.isEmpty(), "Valid EPUB package metadata should parse")
        && expect(data.packagePath == QStringLiteral("OPS/content.opf"), "Package path changed")
        && expect(data.title == QStringLiteral("Test book"), "Title was not normalized")
        && expect(data.author == QStringLiteral("First Author, Second Author"), "Creators were not preserved")
        && expect(data.genre == QStringLiteral("Fiction"), "Subject was not read")
        && expect(data.date == QStringLiteral("2026"), "Date was not read")
        && expect(data.language == QStringLiteral("uk"), "Language was not read")
        && expect(data.annotation == QStringLiteral("A short description"), "Description was not read")
        && expect(data.spinePaths == QStringList({QStringLiteral("OPS/text/chapter-1.xhtml"),
                                                   QStringLiteral("OPS/text/chapter-2.xhtml")}),
                  "Spine paths were not resolved in reading order")
        && expect(data.coverPath == QStringLiteral("OPS/images/cover.jpg"), "EPUB 3 cover should take priority");

    const QByteArray legacyPackage = R"(<package><metadata><meta name="cover" content="cover"/></metadata>
        <manifest><item id="cover" href="cover.png"/></manifest><spine/></package>)";
    const PreviewInternal::EpubPackageData legacy = PreviewInternal::parseEpubPackageData(container, legacyPackage);
    ok = expect(legacy.error.isEmpty(), "EPUB 2 metadata should parse")
        && expect(legacy.coverPath == QStringLiteral("OPS/cover.png"), "EPUB 2 cover was not resolved")
        && ok;

    const QByteArray noCoverPackage = R"(<package><metadata><dc:title xmlns:dc="urn:test">No cover</dc:title></metadata>
        <manifest><item id="chapter" href="chapter.xhtml"/></manifest>
        <spine><itemref idref="chapter"/><itemref idref="missing"/></spine></package>)";
    const PreviewInternal::EpubPackageData noCover = PreviewInternal::parseEpubPackageData(container, noCoverPackage);
    ok = expect(noCover.error.isEmpty(), "An EPUB without a cover should remain valid")
        && expect(noCover.coverPath.isEmpty(), "An absent EPUB cover should stay empty")
        && expect(noCover.spinePaths == QStringList({QStringLiteral("OPS/chapter.xhtml")}),
                  "Missing manifest references should not enter the reading order")
        && ok;

    const QByteArray unsafePackage = R"(<package><metadata/>
        <manifest><item id="chapter" href="../../outside.xhtml"/>
        <item id="cover" href="%2e%2e/%2e%2e/outside.jpg" properties="cover-image"/></manifest>
        <spine><itemref idref="chapter"/></spine></package>)";
    const PreviewInternal::EpubPackageData unsafe = PreviewInternal::parseEpubPackageData(container, unsafePackage);
    ok = expect(unsafe.error.isEmpty(), "Unsafe manifest paths should not invalidate other metadata")
        && expect(unsafe.spinePaths.isEmpty(), "Parent traversal must not enter the EPUB reading order")
        && expect(unsafe.coverPath.isEmpty(), "Parent traversal must not be used as an EPUB cover")
        && ok;

    const QByteArray xhtml = QStringLiteral(
        "<?xml version=\"1.0\"?><html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
        "<h1> Розділ 1 </h1><p>Перший <em>абзац</em>.</p>"
        "<ul><li>Другий пункт</li></ul><div>Ignored block</div>"
        "<blockquote> Цитата\u00a0тут </blockquote></body></html>").toUtf8();
    ok = expect(PreviewInternal::parseEpubXhtmlParagraphs(xhtml)
                    == QStringList({QStringLiteral("Розділ 1"),
                                    QStringLiteral("Перший абзац."),
                                    QStringLiteral("Другий пункт"),
                                    QStringLiteral("Цитата тут")}),
                "EPUB XHTML blocks should preserve normalized reading order")
        && ok;

    const PreviewInternal::EpubPackageData missing = PreviewInternal::parseEpubPackageData(
        QByteArrayLiteral("<container><rootfiles/></container>"), package);
    ok = expect(!missing.error.isEmpty(), "Missing package document should report an error") && ok;

    const PreviewInternal::EpubPackageData malformedContainer = PreviewInternal::parseEpubPackageData(
        QByteArrayLiteral("<container><rootfile"), package);
    ok = expect(malformedContainer.error == QStringLiteral("Cannot parse EPUB container."),
                "Malformed container XML should report a readable error") && ok;

    const PreviewInternal::EpubPackageData unsafeContainer = PreviewInternal::parseEpubPackageData(
        QByteArrayLiteral("<container><rootfiles><rootfile full-path=\"%2e%2e/content.opf\"/>"
                          "</rootfiles></container>"), package);
    ok = expect(!unsafeContainer.error.isEmpty(),
                "Parent traversal must not be accepted as an EPUB package path") && ok;

    const PreviewInternal::EpubPackageData malformedPackage = PreviewInternal::parseEpubPackageData(
        container, QByteArrayLiteral("<package><metadata>"));
    ok = expect(malformedPackage.error == QStringLiteral("Cannot parse EPUB package document."),
                "Malformed package XML should report a readable error") && ok;

    return ok ? 0 : 1;
}
