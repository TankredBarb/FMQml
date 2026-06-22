#!/usr/bin/env bash
set -euo pipefail

ROOT="${1:-src}"
rg -n "QTemporaryFile|QTemporaryDir|QDir::tempPath|QDir::temp\(|\.part|\.fm-tmp|removeRecursively|applicationDirPath" "$ROOT"
