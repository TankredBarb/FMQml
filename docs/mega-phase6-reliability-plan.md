# MEGA Phase 6 Reliability Baseline

Phase 6 now has a safe baseline that makes the MEGA provider more resilient
without expanding the MEGA UX scope into direct document creation, rename, or
direct move workflows.

## Goals

- Detect external MEGA account changes without forcing immediate full-account
  rescans.
- Keep the current cached view usable until the user explicitly refreshes.
- Avoid blocking the UI thread with retry sleeps, synchronous preview downloads,
  or automatic cache rebuilds.
- Add secret-safe diagnostics for MEGA logs and tests.

## Implemented Policy

1. **Remote change bridge**
   - Listen for account node updates after sign-in/session resume.
   - Mark the account cache dirty when an external update arrives.
   - Emit a concise status message such as `MEGA changed remotely; refresh to
     update.`
   - Do not call `scan()` automatically from the node-update callback.

2. **Refresh behavior**
   - Cached account folders remain usable for normal navigation after a
     remote-change notification.
   - The next explicit refresh of a dirty account path reloads the account tree
     through the SDK.
   - Provider-originated uploads/deletes update the local cache directly and
     should not force a full account reload.
   - Provider-originated mutations must not clear a dirty marker that was set by
     a prior external account update.

3. **Transfers**
   - Do not add retry/backoff with `QThread::msleep()` in provider methods.
   - Keep transfer retries as future async work owned by operation jobs or the
     SDK bridge, not by synchronous file-provider calls.
   - Keep existing cleanup behavior for `.part` files and preview staging.
   - Treat async retry/backoff and persisted SDK resume as deferred work until
     the operation queue can represent retry state without blocking provider
     calls or the UI thread.

4. **Previews**
   - Do not disable `openRead` fallback as a reliability fix.
   - Full-file fallback remains bounded by the existing safety limit and cleanup
     leases until an SDK preview/thumbnail bridge replaces it.

5. **Diagnostics**
   - Redact public-link keys, session tokens, passwords, generic token/key
     values, and SDK state paths before writing MEGA diagnostics.
   - Route MEGA transfer warning logs through the redaction helper before
     printing local SDK paths or SDK-provided error text.

## Acceptance Checks

- A remote account-change notification emits a user-visible status message.
- The notification does not automatically call `loadAccountRoot()` or rescan the
  current folder.
- Normal navigation after the notification keeps using cached account children.
- An explicit refresh after the notification reloads stale account children.
- MEGA diagnostics do not expose link keys, session tokens, or SDK state paths.

## Deferred Work

- Async retry/backoff for transfers.
- Persisted transfer resume, if SDK state can be validated safely.
- SDK thumbnail/preview bridge to replace bounded full-file preview
  materialization.
