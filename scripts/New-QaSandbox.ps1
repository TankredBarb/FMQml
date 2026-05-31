param(
    [string]$Root = "D:\QASandbox",
    [int]$BulkCount = 250,
    [int]$NestedDepth = 4,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function New-TextFile {
    param(
        [string]$Path,
        [string]$Content
    )

    $directory = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Path $directory | Out-Null
    }

    Set-Content -LiteralPath $Path -Value $Content -Encoding UTF8
}

function Assert-SafeCleanTarget {
    param([string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $rootPath = [System.IO.Path]::GetPathRoot($fullPath)
    $leaf = Split-Path -Leaf $fullPath

    if ($fullPath -eq $rootPath) {
        throw "Refusing to clean drive root: $fullPath"
    }

    $allowedNames = @("QASandbox", "FMQml-QA-Sandbox")
    if ($allowedNames -notcontains $leaf) {
        throw "Refusing to clean '$fullPath'. Use D:\QASandbox or a folder named FMQml-QA-Sandbox."
    }
}

if ($BulkCount -lt 0) {
    throw "BulkCount must be non-negative."
}

if ($NestedDepth -lt 0) {
    throw "NestedDepth must be non-negative."
}

if ($Clean -and (Test-Path -LiteralPath $Root)) {
    Assert-SafeCleanTarget -Path $Root
    Remove-Item -LiteralPath $Root -Recurse -Force
}

New-Item -ItemType Directory -Path $Root -Force | Out-Null

$folders = @(
    "Alpha",
    "Beta",
    "Copy Source",
    "Copy Target",
    "Move Source",
    "Move Target",
    "Rename Single",
    "Rename Batch",
    "Delete Target",
    "WatcherTarget",
    "DeleteMeCurrent",
    "ReadOnly Area",
    "Unicode Names"
)

foreach ($folder in $folders) {
    New-Item -ItemType Directory -Path (Join-Path $Root $folder) -Force | Out-Null
}

New-TextFile -Path (Join-Path $Root "Alpha\alpha.txt") -Content "Alpha text file"
New-TextFile -Path (Join-Path $Root "Alpha\alpha.log") -Content "Alpha log file"
New-TextFile -Path (Join-Path $Root "Beta\beta.txt") -Content "Beta text file"
New-TextFile -Path (Join-Path $Root "Copy Source\copy-one.txt") -Content "Copy one"
New-TextFile -Path (Join-Path $Root "Copy Source\copy two.txt") -Content "Copy two"
New-TextFile -Path (Join-Path $Root "Move Source\move-one.txt") -Content "Move one"
New-TextFile -Path (Join-Path $Root "Rename Single\plain-name.txt") -Content "Rename me"
New-TextFile -Path (Join-Path $Root "Rename Single\case_only.txt") -Content "Case-only rename target"
New-TextFile -Path (Join-Path $Root "Rename Single\conflict-source.txt") -Content "Conflict source"
New-TextFile -Path (Join-Path $Root "Rename Single\conflict-target.txt") -Content "Conflict target"
New-TextFile -Path (Join-Path $Root "Delete Target\delete-one.txt") -Content "Delete one"
New-TextFile -Path (Join-Path $Root "Delete Target\delete two.txt") -Content "Delete two"
New-TextFile -Path (Join-Path $Root "WatcherTarget\watch-existing.txt") -Content "Watcher existing"
New-TextFile -Path (Join-Path $Root "Unicode Names\accented-name.txt") -Content "Unicode name placeholder"
New-TextFile -Path (Join-Path $Root "file with spaces.txt") -Content "Spaces in filename"

for ($i = 1; $i -le 12; $i++) {
    $name = "batch-{0:D2}.txt" -f $i
    New-TextFile -Path (Join-Path $Root "Rename Batch\$name") -Content "Batch rename file $i"
}

$nestedRoot = Join-Path $Root "Nested"
$current = $nestedRoot
for ($i = 1; $i -le $NestedDepth; $i++) {
    $current = Join-Path $current ("Level-{0:D2}" -f $i)
    New-Item -ItemType Directory -Path $current -Force | Out-Null
    New-TextFile -Path (Join-Path $current ("nested-{0:D2}.txt" -f $i)) -Content "Nested level $i"
}

$bulkRoot = Join-Path $Root "Bulk"
New-Item -ItemType Directory -Path $bulkRoot -Force | Out-Null
for ($i = 1; $i -le $BulkCount; $i++) {
    $fileName = "bulk-{0:D5}.txt" -f $i
    New-TextFile -Path (Join-Path $bulkRoot $fileName) -Content "Bulk file $i"
}

$readOnlyRoot = Join-Path $Root "ReadOnly Area"
New-TextFile -Path (Join-Path $readOnlyRoot "read-only-file.txt") -Content "Read-only sample"
$readOnlyItem = Get-Item -LiteralPath (Join-Path $readOnlyRoot "read-only-file.txt")
$readOnlyItem.Attributes = $readOnlyItem.Attributes -bor [System.IO.FileAttributes]::ReadOnly

Write-Host "QA sandbox created:"
Write-Host $Root
Write-Host ""
Write-Host "Useful folders:"
Write-Host "  Rename Single:  $(Join-Path $Root 'Rename Single')"
Write-Host "  Rename Batch:   $(Join-Path $Root 'Rename Batch')"
Write-Host "  Delete Target:  $(Join-Path $Root 'Delete Target')"
Write-Host "  WatcherTarget:  $(Join-Path $Root 'WatcherTarget')"
Write-Host "  Bulk:           $(Join-Path $Root 'Bulk')"
