param(
    [string]$DriverPath = "$(Split-Path -Path $MyInvocation.MyCommand.Definition -Parent)\ram_dedupe.sys",
    [string]$CertificatePath = "",
    [SecureString]$CertificatePassword = $null,
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

if (-not (Test-Path $DriverPath)) {
    Write-Error "Driver binary not found: $DriverPath"
    exit 1
}

if (-not (Test-Path $CertificatePath)) {
    Write-Error "Certificate file not found: $CertificatePath"
    exit 1
}

$signtoolPath = Get-Command signtool.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1
if (-not $signtoolPath) {
    Write-Error "signtool.exe was not found in PATH. Install Windows SDK or add signtool to PATH."
    exit 1
}

$arguments = @(
    "sign",
    "/fd", "SHA256",
    "/td", "SHA256",
    "/tr", $TimestampUrl,
    "/f", $CertificatePath
)

if ($CertificatePassword) {
    $plainPassword = [Runtime.InteropServices.Marshal]::PtrToStringAuto(
        [Runtime.InteropServices.Marshal]::SecureStringToBSTR($CertificatePassword)
    )
    $arguments += "/p"
    $arguments += $plainPassword
}

$arguments += $DriverPath

$process = Start-Process -FilePath $signtoolPath -ArgumentList $arguments -NoNewWindow -Wait -PassThru
if ($process.ExitCode -ne 0) {
    Write-Error "signtool failed with exit code $($process.ExitCode)"
    exit $process.ExitCode
}

Write-Host "Driver signed successfully: $DriverPath"
