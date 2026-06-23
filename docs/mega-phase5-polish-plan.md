# MEGA Phase 5 Polish Plan

Phase 5 makes the MEGA provider feel native in the file panel without expanding the MEGA scope into direct document creation or rename workflows.

## Goals

- Populate MEGA `FileEntry` presentation metadata consistently with local and Google Drive entries:
  - shared file-type icon names for files and folders;
  - MIME type from suffix where the SDK does not provide a better value;
  - image and thumbnail eligibility flags for preview-capable media.
- Prefer SDK thumbnails/previews before full-file fallback downloads when the client bridge grows thumbnail APIs.
- Keep every fallback preview/materialization artifact under `CleanupSubsystem` leases.
- Keep public-link keys out of provider paths, logs, dialogs, and history.
- Add safe link-oriented actions only when they can be implemented without exposing secrets.

## Implementation slices

1. **Presentation metadata baseline**
   - Enrich account scans, public-link scans, fresh uploads, and internal folder creates with native file-type icon names and MIME/thumbnail flags.
   - Keep MEGA-specific roots mapped to existing MEGA assets (`mega`, `mega-clouddrive`) and use shared folder/file assets everywhere else.

2. **Preview fallback safety**
   - Continue routing `openRead` staging through `CleanupSubsystem` as `RemotePreview` artifacts.
   - Full-file fallback materialization must refuse files above the configured safety limit until SDK preview/thumbnail retrieval exists.
   - Any future provider-to-provider preview payload or thumbnail fallback must register a cleanup lease before network transfer begins.

3. **SDK preview bridge**
   - Add client interface methods for SDK thumbnail/preview retrieval.
   - Cache thumbnail files by provider path, MEGA handle, requested size, and modified timestamp.
   - Fall back to full-file materialization only for files below the configured Quick Look safety threshold.

4. **Safe link actions**
   - Offer `copyLink` / `openInBrowser` only for account nodes after the SDK returns a public link.
   - Never reconstruct public-link URLs from cached public-link keys for UI display unless the user explicitly supplied the link in that action context.
   - Treat `importLink` as a later opt-in action because it mutates account storage.

## Acceptance checks

- MEGA files show the same bundled/native icon class as equivalent local files.
- Regression coverage checks public-link file metadata and freshly uploaded account-file metadata.
- MEGA image/video candidates advertise thumbnail eligibility to the existing thumbnail pipeline.
- Preview fallback failures, cancellations, and over-limit skips leave no unmanaged temporary files.
- Public-link paths still reject mutation operations and do not expose keys in `FileEntry::path`.
