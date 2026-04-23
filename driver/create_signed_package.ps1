param(
    [string]$CertificatePath,
    [SecureString]$CertificatePassword = $null,
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [string]$OutputFolder = "$(Split-Path -Path $MyInvocation.MyCommand.Definition -Parent)\package"
)

if (-not (Test-Path $CertificatePath)) {
    Write-Error "Certificate file not found: $CertificatePath"
    exit 1
}

$scriptDir = Split-Path -Path $MyInvocation.MyCommand.Definition -Parent
$driverPath = Join-Path $scriptDir "ram_dedupe.sys"

if (-not (Test-Path $driverPath)) {
    Write-Error "Driver binary not found: $driverPath"
    exit 1
}

if (-not $CertificatePassword) {
    Write-Host "Certificate password not supplied. Prompting securely."
    $CertificatePassword = Read-Host -AsSecureString "Enter certificate password"
}

Write-Host "Signing driver using certificate: $CertificatePath"
& "$scriptDir\sign_driver.ps1" -DriverPath $driverPath -CertificatePath $CertificatePath -CertificatePassword $CertificatePassword -TimestampUrl $TimestampUrl
if ($LASTEXITCODE -ne 0) {
    Write-Error "Driver signing failed"
    exit $LASTEXITCODE
}

Write-Host "Packaging signed driver into ZIP archive"
& "$scriptDir\package_driver.ps1" -OutputFolder $OutputFolder
if ($LASTEXITCODE -ne 0) {
    Write-Error "Driver packaging failed"
    exit $LASTEXITCODE
}

$zipFiles = Get-ChildItem -Path $scriptDir -Filter "ram_dedupe_package_*.zip" | Sort-Object LastWriteTime -Descending
if ($zipFiles.Count -eq 0) {
    Write-Error "No package archive found after packaging"
    exit 1
}

Write-Host "Signed driver package created successfully: $($zipFiles[0].FullName)"
