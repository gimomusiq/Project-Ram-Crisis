param(
    [string]$InfPath = "$(Split-Path -Path $MyInvocation.MyCommand.Definition -Parent)\ram_dedupe.inf",
    [string]$ServiceName = "ram_dedupe"
)

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "Administrator privileges are required to uninstall the driver."
    exit 1
}

Write-Host "Stopping driver service '$ServiceName' if it exists..."
$service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($service) {
    if ($service.Status -eq 'Running') {
        Stop-Service -Name $ServiceName -Force -ErrorAction SilentlyContinue
    }
    sc.exe delete $ServiceName | Out-Null
}

if (-not (Test-Path $InfPath)) {
    Write-Error "Driver INF file not found: $InfPath"
    exit 1
}

Write-Host "Removing driver package from the driver store..."
$process = Start-Process -FilePath pnputil -ArgumentList "/delete-driver `"$InfPath`" /uninstall /force" -NoNewWindow -Wait -PassThru
if ($process.ExitCode -ne 0) {
    Write-Error "pnputil failed with exit code $($process.ExitCode)"
    exit $process.ExitCode
}

Write-Host "Driver removal completed successfully."
