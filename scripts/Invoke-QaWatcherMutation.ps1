param(
    [string]$Target = "D:\QASandbox\WatcherTarget",
    [int]$BulkCount = 500,
    [int]$DelayMilliseconds = 50
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $Target)) {
    New-Item -ItemType Directory -Path $Target -Force | Out-Null
}

function Write-Sample {
    param(
        [string]$Path,
        [string]$Content
    )

    Set-Content -LiteralPath $Path -Value $Content -Encoding UTF8
}

$created = Join-Path $Target "external-created.txt"
$renamed = Join-Path $Target "external-renamed.txt"
$temporary = Join-Path $Target "external-temporary.txt"
$replace = Join-Path $Target "external-replace.txt"
$bulkRoot = Join-Path $Target "ExternalBulk"

Write-Sample -Path $created -Content "Created outside FMQml"
Start-Sleep -Milliseconds $DelayMilliseconds

if (Test-Path -LiteralPath $renamed) {
    Remove-Item -LiteralPath $renamed -Force
}
Rename-Item -LiteralPath $created -NewName (Split-Path -Leaf $renamed)
Start-Sleep -Milliseconds $DelayMilliseconds

Write-Sample -Path $temporary -Content "Short-lived file"
Start-Sleep -Milliseconds $DelayMilliseconds
Remove-Item -LiteralPath $temporary -Force
Start-Sleep -Milliseconds $DelayMilliseconds

Write-Sample -Path $replace -Content "Original content"
Start-Sleep -Milliseconds $DelayMilliseconds
Remove-Item -LiteralPath $replace -Force
Start-Sleep -Milliseconds $DelayMilliseconds
Write-Sample -Path $replace -Content "Replacement content"
Start-Sleep -Milliseconds $DelayMilliseconds

New-Item -ItemType Directory -Path $bulkRoot -Force | Out-Null
for ($i = 1; $i -le $BulkCount; $i++) {
    $name = "external-bulk-{0:D5}.txt" -f $i
    Write-Sample -Path (Join-Path $bulkRoot $name) -Content "External bulk $i"
}
Start-Sleep -Milliseconds $DelayMilliseconds
Remove-Item -LiteralPath $bulkRoot -Recurse -Force

Write-Host "External watcher mutations finished:"
Write-Host $Target
