param(
    [string]$OutputFolder = "$(Split-Path -Path $MyInvocation.MyCommand.Definition -Parent)\package"
)

$scriptDir = Split-Path -Path $MyInvocation.MyCommand.Definition -Parent
$sysFile = Join-Path $scriptDir "ram_dedupe.sys"
$infFile = Join-Path $scriptDir "ram_dedupe.inf"
$readmeFile = Join-Path $scriptDir "README.md"
$releaseDocsFile = Join-Path $scriptDir "README.md"
$packageManifest = Join-Path $OutputFolder "package_manifest.json"

if (-not (Test-Path $sysFile)) {
    Write-Error "Driver binary not found: $sysFile"
    exit 1
}

if (-not (Test-Path $infFile)) {
    Write-Error "Driver INF file not found: $infFile"
    exit 1
}

if (-not (Test-Path $readmeFile)) {
    Write-Error "Driver README not found: $readmeFile"
    exit 1
}

New-Item -ItemType Directory -Path $OutputFolder -Force | Out-Null
Copy-Item -Path $sysFile -Destination $OutputFolder -Force
Copy-Item -Path $infFile -Destination $OutputFolder -Force
Copy-Item -Path $readmeFile -Destination $OutputFolder -Force

$manifest = [ordered]@{
    packageName = "ram_dedupe"
    packageTimestamp = (Get-Date).ToString("o")
    packageFiles = @(
        "ram_dedupe.sys",
        "ram_dedupe.inf",
        "README.md"
    )
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -Path $packageManifest -Encoding UTF8

$zipPath = Join-Path $scriptDir "ram_dedupe_package_$(Get-Date -Format 'yyyyMMdd_HHmmss').zip"
Compress-Archive -Path "$OutputFolder\*" -DestinationPath $zipPath -Force
Write-Host "Driver package created: $zipPath"
Write-Host "Package manifest generated: $packageManifest"
