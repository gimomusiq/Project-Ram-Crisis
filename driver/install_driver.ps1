param(
    [string]$InfPath = "$(Split-Path -Path $MyInvocation.MyCommand.Definition -Parent)\ram_dedupe.inf",
    [string]$ServiceName = "ram_dedupe"
)

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "Administrator privileges are required to install the driver."
    exit 1
}

if (-not (Test-Path $InfPath)) {
    Write-Error "Driver INF file not found: $InfPath"
    exit 1
}

Write-Host "Installing RAM Dedupe driver from $InfPath..."
$existing = pnputil /enum-drivers | Select-String -Pattern [regex]::Escape((Split-Path -Leaf $InfPath))
if ($existing) {
    Write-Host "Driver package already present in the driver store. Attempting to install if needed..."
}

$process = Start-Process -FilePath pnputil -ArgumentList "/add-driver `"$InfPath`" /install" -NoNewWindow -Wait -PassThru
if ($process.ExitCode -ne 0) {
    Write-Error "pnputil failed with exit code $($process.ExitCode)"
    exit $process.ExitCode
}

Write-Host "Driver package installed successfully."

$service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($service) {
    if ($service.Status -ne 'Running') {
        Write-Host "Starting driver service '$ServiceName'..."
        Start-Service -Name $ServiceName -ErrorAction Stop
        Write-Host "Driver service '$ServiceName' started."
    } else {
        Write-Host "Driver service '$ServiceName' is already running."
    }
} else {
    Write-Warning "Driver service '$ServiceName' was not found after installation. Verify INF AddService configuration."
}

Write-Host "Driver installation completed successfully."
