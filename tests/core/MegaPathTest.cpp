#include "MegaPath.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << "FAILED: " << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // 1. Test isSchemePath
    if (!MegaPath::isSchemePath("mega:///")) {
        return fail("mega:/// should be identified as scheme path");
    }
    if (!MegaPath::isSchemePath("MEGA://Cloud Drive")) {
        return fail("MEGA:// should be identified as scheme path (case insensitive)");
    }
    if (MegaPath::isSchemePath("gdrive://my-drive")) {
        return fail("gdrive:// should not be identified as mega scheme path");
    }
    if (MegaPath::isSchemePath("/home/user")) {
        return fail("absolute local path should not be identified as scheme path");
    }

    // 2. Test normalizedPath
    if (MegaPath::normalizedPath("mega:") != MegaPath::Root) {
        return fail("mega: should normalize to mega:///");
    }
    if (MegaPath::normalizedPath("mega://") != MegaPath::Root) {
        return fail("mega:// should normalize to mega:///");
    }
    if (MegaPath::normalizedPath("mega:///") != MegaPath::Root) {
        return fail("mega:/// should normalize to mega:///");
    }
    if (MegaPath::normalizedPath("mega://\\Cloud Drive\\Subfolder\\\\File.txt") != "mega:///Cloud Drive/Subfolder/File.txt") {
        return fail("slashes and double slashes should be normalized correctly");
    }
    if (MegaPath::normalizedPath("mega:///link/abc") != "mega://link/abc") {
        return fail("mega:///link/abc should normalize to mega://link/abc");
    }
    if (MegaPath::normalizedPath("mega://link/abc/def/") != "mega://link/abc/def") {
        return fail("trailing slashes in link paths should be stripped");
    }

    // 3. Test parentPath
    if (!MegaPath::parentPath(MegaPath::Root).isEmpty()) {
        return fail("parent of mega:/// should be empty");
    }
    if (MegaPath::parentPath("mega:///Cloud Drive") != MegaPath::Root) {
        return fail("parent of mega:///Cloud Drive should be mega:///");
    }
    if (MegaPath::parentPath("mega:///Cloud Drive/Folder/Subfolder") != "mega:///Cloud Drive/Folder") {
        return fail("parent path of nested folder should be correct");
    }
    if (MegaPath::parentPath("mega://link/abc") != MegaPath::Root) {
        return fail("parent of mega://link/abc should be mega:///");
    }
    if (MegaPath::parentPath("mega://link/abc/Subfolder") != "mega://link/abc") {
        return fail("parent of nested link path should preserve link scheme");
    }

    // 4. Test childPath
    if (MegaPath::childPath(MegaPath::Root, "Cloud Drive") != "mega:///Cloud Drive") {
        return fail("child of mega:/// and Cloud Drive should be mega:///Cloud Drive");
    }
    if (MegaPath::childPath("mega:///Cloud Drive", "Photos") != "mega:///Cloud Drive/Photos") {
        return fail("child of mega:///Cloud Drive and Photos should be mega:///Cloud Drive/Photos");
    }
    if (MegaPath::childPath("mega:///Cloud Drive/", "Photos") != "mega:///Cloud Drive/Photos") {
        return fail("child path should handle trailing slash in parent path");
    }

    // 5. Test fallbackFileNameForPath
    if (MegaPath::fallbackFileNameForPath(MegaPath::Root) != "MEGA") {
        return fail("fallback filename for mega:/// should be MEGA");
    }
    if (MegaPath::fallbackFileNameForPath("mega:///Cloud Drive") != "Cloud Drive") {
        return fail("fallback filename for mega:///Cloud Drive should be Cloud Drive");
    }
    if (MegaPath::fallbackFileNameForPath("mega://link/abc") != "abc") {
        return fail("fallback filename for mega://link/abc should be abc");
    }
    if (MegaPath::fallbackFileNameForPath("mega://link/abc/Folder/file.zip") != "file.zip") {
        return fail("fallback filename for mega://link/abc/Folder/file.zip should be file.zip");
    }

    // 6. Test fromUserInput
    QString linkId, linkKey;
    bool isFolder = false;

    // Test modern folder link
    QString path = MegaPath::fromUserInput("https://mega.nz/folder/abc#key", linkId, linkKey, isFolder);
    if (path != "mega://link/abc" || !isFolder || linkId != "abc" || linkKey != "key") {
        return fail("Failed parsing modern folder link");
    }

    // Test modern file link with query param cleanup
    path = MegaPath::fromUserInput("https://mega.nz/file/xyz#secret?foo=bar", linkId, linkKey, isFolder);
    if (path != "mega://link/xyz" || isFolder || linkId != "xyz" || linkKey != "secret") {
        return fail("Failed parsing modern file link");
    }

    // Test legacy folder link
    path = MegaPath::fromUserInput("http://www.mega.nz/#F!123!456", linkId, linkKey, isFolder);
    if (path != "mega://link/123" || !isFolder || linkId != "123" || linkKey != "456") {
        return fail("Failed parsing legacy folder link");
    }

    // Test legacy file link
    path = MegaPath::fromUserInput("mega.nz/#!789!abc", linkId, linkKey, isFolder);
    if (path != "mega://link/789" || isFolder || linkId != "789" || linkKey != "abc") {
        return fail("Failed parsing legacy file link");
    }

    // Test already parsed path
    path = MegaPath::fromUserInput("mega://link/abc/def", linkId, linkKey, isFolder);
    if (path != "mega://link/abc/def" || !linkId.isEmpty() || !linkKey.isEmpty()) {
        return fail("Already normalized link path should be preserved but return empty parsed fields");
    }

    // Test invalid link
    path = MegaPath::fromUserInput("https://google.com", linkId, linkKey, isFolder);
    if (!path.isEmpty()) {
        return fail("Invalid MEGA link should return empty path");
    }

    QTextStream(stdout) << "All MegaPath unit tests passed successfully!\n";
    return 0;
}
