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

    const PreviewInternal::EpubPackageData missing = PreviewInternal::parseEpubPackageData(
        QByteArrayLiteral("<container><rootfiles/></container>"), package);
    ok = expect(!missing.error.isEmpty(), "Missing package document should report an error") && ok;

    return ok ? 0 : 1;
}
