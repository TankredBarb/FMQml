# Temporary artifact cleanup audit

This inventory tracks the FM-owned temporary or staging artifacts reviewed after
introducing `CleanupSubsystem`. Runtime payload temps should either be
cleanup-managed or explicitly documented as non-payload exemptions.

## Covered paths

| File | Artifact pattern | Cleanup coverage |
| --- | --- | --- |
| `src/core/OperationQueue.cpp` | local copy `.part`, provider-transfer staging | Uses `CleanupSubsystem` leases for operation payload staging. |
| `src/core/TerminalLauncher.cpp` | `fmqml-yakuake-session-*` | Creates Yakuake session scripts under the default cleanup root and registers a `YakuakeSession` lease. Error paths schedule lease deletion; successful launches schedule delayed deletion while startup cleanup covers crashes before the delay fires. |
| `src/core/ArchiveFileProvider.cpp` | `.fm-nested-*`, `.fm-read-*`, `.fm-extract-*`, `.fm-7z-extract-*` | Extraction temps are registered as recursive `ArchiveExtract` leases. Nested/read browse temps are registered as `ArchiveBrowse` leases and no longer fall back to anonymous system-temp directories when no cleanup parent can be resolved. |
| `src/controllers/QuickLookController.cpp` | `remote-preview/<uuid>` | Uses the default cleanup root, registers recursive `RemotePreview` leases, and schedules lease deletion when preview state is cleared or abandoned. |
| `src/core/ThumbnailProvider.cpp` | `fm-archive-thumb-*`, `fm-thumb-*` | Adapter files use the default cleanup root when available, disable implicit auto-remove, and register `ThumbnailAdapter` leases for cleanup. The last-resort system-temp fallback remains immediate local removal only because the cleanup subsystem intentionally rejects the global temp root as a safety root. |
| `src/plugins/portable_device/PortableDeviceFileProviderPlugin.cpp` | `fm-portable-*` preview adapter | Preview adapter files use caller/default cleanup staging when available and are wrapped in a cleanup-managed temporary file that schedules the registered lease when the device is destroyed. Last-resort system-temp fallback is removed directly by the wrapper. |
| `src/plugins/mega/MegaFileProviderPlugin.cpp` | MEGA preview adapter files | Already uses a cleanup-managed temporary file wrapper and `RemotePreview` leases. |
| `src/core/IconProvider.cpp` | fake extension paths under `QDir::temp()`/`QDir::tempPath()` | Explicitly exempt: these are Windows Shell identity strings used with `SHGFI_USEFILEATTRIBUTES`; no FM payload file is created. |
| `src/core/LaunchService.cpp` | Proton `logs`/`compatdata` fallback base | Explicitly exempt: these are persistent compatibility/app-data artifacts, not transient payload staging. |
| `src/core/CleanupSubsystem.cpp` | `.fm-cleanup-probe-*`, `.fm-tmp/<operation-id>` | Exempt as the cleanup subsystem implementation itself. |

## Audit command

Use `scripts/audit_temp_files.sh src` for a broad pattern scan. Every hit should
be classified as one of: cleanup-managed, short-lived direct-remove fallback,
persistent app data, fake API identity path, test-only, or migration candidate.
