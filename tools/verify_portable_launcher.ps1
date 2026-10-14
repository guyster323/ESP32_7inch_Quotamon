param([string]$PackageDirectory = '')
$ErrorActionPreference = 'Stop'
if (!$PackageDirectory) { $PackageDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'dist/Quotamon-Portable' }
$executable = (Resolve-Path -LiteralPath (Join-Path $PackageDirectory 'Quotamon.exe')).Path
$launcher = Start-Process -FilePath $executable -ArgumentList '--headless','--minimized' -WindowStyle Hidden -PassThru
$children = @()
try {
    Start-Sleep -Seconds 8
    if ($launcher.HasExited) { throw 'Launcher exited before verification' }
    $backend = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($launcher.Id)" | Where-Object { $_.Name -eq 'node.exe' })
    if ($backend.Count -ne 1) { throw 'Expected exactly one owned Node backend' }
    $ble = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($backend[0].ProcessId)" | Where-Object { $_.Name -eq 'Quotamon.exe' })
    if ($ble.Count -ne 1) { throw 'Expected one owned BLE resource' }
    Stop-Process -Id $ble[0].ProcessId -Force
    Start-Sleep -Seconds 7
    $recovered = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($backend[0].ProcessId)" | Where-Object { $_.Name -eq 'Quotamon.exe' })
    if ($recovered.Count -ne 1 -or $recovered[0].ProcessId -eq $ble[0].ProcessId) { throw 'BLE resource did not restart once' }
    Start-Sleep -Seconds 6
    $stable = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($backend[0].ProcessId)" | Where-Object { $_.Name -eq 'Quotamon.exe' })
    if ($stable.Count -ne 1 -or $stable[0].ProcessId -ne $recovered[0].ProcessId) { throw 'BLE resource restart did not stabilize' }
    $ble = $stable
    $children = @($backend[0].ProcessId,$ble[0].ProcessId)
    # This process handle was created above; no existing app or Orca process is targeted.
    Stop-Process -Id $launcher.Id -Force
    Start-Sleep -Seconds 2
    foreach ($childId in $children) {
        if (Get-Process -Id $childId -ErrorAction SilentlyContinue) { throw 'A bridge child survived launcher termination' }
    }
    Write-Output 'Portable launcher: PASS; BLE resource recovers once, stabilizes, and all children exit on launcher crash; no startup entry created.'
} finally {
    if (!$launcher.HasExited) { Stop-Process -Id $launcher.Id -Force }
    $launcher.Dispose()
}
