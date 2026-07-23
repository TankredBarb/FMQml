# EPUB Preview and Quick Look Plan

## Goal

Add `.epub` support that matches the existing FB2 experience:

- book card in the Preview Pane;
- title, author, cover, and available metadata;
- cover thumbnail in file views;
- paginated, adjustable-font reading in Quick Look.

The existing `BookPreview.qml` and book state in `QuickLookController` remain the shared UI and reader model. EPUB should supply the same data contract as FB2 rather than introduce a second reader UI.

## Scope and Non-Goals

An EPUB is a ZIP container. The implementation must read that container internally, but it must not add `.epub` to the general archive-extension list: doing so would change normal file-manager behaviour by exposing books as browseable archives.

Initial support is a reflowed text reader, matching current FB2 semantics. It deliberately excludes:

- rendering the publisher's CSS/layout;
- inline images, hyperlinks, notes, and interactive content;
- table of contents navigation;
- saved reading position.

## Implementation Plan

### 1. Define fixtures and expected behaviour

Prepare small, versioned test EPUB fixtures covering:

- EPUB 2 and EPUB 3;
- book with a cover and book without one;
- multiple spine documents, Cyrillic text, and metadata;
- a malformed container or missing OPF document.

For every fixture record the expected title, author, optional cover, metadata, and reading order. The malformed fixture must produce a readable failure message without crashing or blocking Quick Look.

### 2. Create a dedicated EPUB loader

Add `EpubPreviewLoader.{h,cpp}` beside `Fb2PreviewLoader` and include it through `PreviewInternal.h` and `CMakeLists.txt`.

Its responsibilities are:

1. Open the EPUB as a ZIP internally using the existing archive backend without registering `.epub` as a generic archive type.
2. Read `META-INF/container.xml` and resolve the package document (OPF).
3. Parse OPF metadata into the existing book fields: `Title`, `Author`, `Genre`, `Date`, `Language`, and `Annotation` when present.
4. Build the manifest and follow `spine/itemref` order to load XHTML content documents.
5. Convert readable block text (`h1`-`h6`, `p`, `li`, `blockquote`, and equivalent safe fallbacks) into normalized paragraphs.
6. Locate the cover through EPUB 3 `properties="cover-image"`, then the EPUB 2 `meta name="cover"` fallback.
7. Return the same pages, paragraphs, title, author, metadata, cover source, and error-state fields used by `Fb2PreviewData`.

Use the existing page-size calculation and page builder for the returned paragraphs, so font-size changes preserve the current proportional page-position behaviour.

### 3. Integrate the book data into previews

Recognize `.epub` in `PreviewClassifier` and the preview-loading paths as a `book` with MIME type `application/epub+zip`.

Wire the EPUB loader into both paths already used by FB2:

- the lightweight Preview Pane metadata load;
- the asynchronous Quick Look content load and deferred full-book load.

Keep `BookPreview.qml` unchanged unless a genuine EPUB-only data gap appears: it already derives the displayed format from the extension and consumes the shared book properties.

### 4. Add cover thumbnails and fallback icon

Extend `ThumbnailProvider` with an EPUB-cover extraction path analogous to the FB2 `::cover` request. It must decode the selected cover directly from the EPUB container and return an empty image when none is available, allowing the normal fallback icon to remain visible.

Register EPUB in `FileTypeIconResolver` with the book icon, so books without embedded covers are still recognizable in file views.

### 5. Validate behaviour

Add focused loader tests for container resolution, OPF metadata, spine order, both cover conventions, absent covers, and malformed EPUB handling.

Then verify:

1. `cmake --build build -j 12` succeeds.
2. The focused test target(s) pass and `git diff --check` is clean.
3. Manual Preview Pane and Quick Look checks confirm a cover book, a no-cover book, page navigation, font-size re-pagination, and an invalid EPUB.

## Acceptance Criteria

- A local `.epub` is classified as a book, not as a generic archive.
- Preview Pane shows available metadata and a cover or the normal book fallback.
- Quick Look can read all spine documents in order, navigate pages, and adjust reader font size.
- EPUB 2 and EPUB 3 cover conventions work where the source contains a decodable image.
- Missing or malformed EPUB data produces a clear preview state and does not affect FB2 or normal archive behaviour.
