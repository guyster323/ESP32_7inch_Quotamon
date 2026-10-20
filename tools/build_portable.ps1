param([string]$Dotnet = 'dotnet', [string]$Node = 'node', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'dist/Quotamon-Portable' }
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
if (Test-Path -LiteralPath (Join-Path $outputRoot '_internal')) { throw 'Select a clean output folder; this folder contains an earlier experimental package.' }
$nodeExecutable = (Get-Command $Node -ErrorAction Stop).Source
$dotnetExecutable = (Get-Command $Dotnet -ErrorAction Stop).Source
# Use esbuild's JavaScript API so this works with its native Windows executable.
# Windows PowerShell treats ordinary native stderr (including esbuild progress)
# as an ErrorRecord. Native tools report failure through their exit code.
function Invoke-BuildTool([string]$Executable, [string[]]$Arguments) {
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $Executable @Arguments 2>&1 | ForEach-Object { Write-Output "$_" }
        $toolExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
    if ($toolExit -ne 0) { throw "Build tool failed ($toolExit): $Executable" }
}
Invoke-BuildTool $nodeExecutable @((Join-Path $PSScriptRoot 'bundle_portable.mjs'), (Join-Path $outputRoot 'bridge.cjs'))
Invoke-BuildTool $dotnetExecutable @('publish', (Join-Path $projectRoot 'portable/Quotamon.Portable.csproj'), '-c', 'Release', '-o', $outputRoot)
Copy-Item -LiteralPath $nodeExecutable -Destination (Join-Path $outputRoot 'node.exe') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'portable/web') -Destination $outputRoot -Recurse -Force
$assetRoot = Join-Path $outputRoot 'assets'
foreach ($provider in @('codex','cursor','copilot','claude','grok')) {
    New-Item -ItemType Directory -Path (Join-Path $assetRoot $provider) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot "assets/providers/$provider/idle_00.png") -Destination (Join-Path $assetRoot $provider) -Force
}
$noticeRoot = Join-Path $outputRoot 'licenses'
New-Item -ItemType Directory -Path $noticeRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination $noticeRoot -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination $noticeRoot -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets/providers/F108pro-LICENSE.txt') -Destination $noticeRoot -Force
Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $dotnetExecutable) 'LICENSE.txt') -Destination (Join-Path $noticeRoot 'DOTNET-LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $dotnetExecutable) 'ThirdPartyNotices.txt') -Destination (Join-Path $noticeRoot 'DOTNET-THIRD-PARTY.txt') -Force
if (Test-Path -LiteralPath (Join-Path $outputRoot 'Quotamon.pdb')) { Remove-Item -LiteralPath (Join-Path $outputRoot 'Quotamon.pdb') -Force }
Copy-Item -LiteralPath (Join-Path $projectRoot 'portable/QUICKSTART.txt') -Destination $outputRoot -Force
$nodeLicense = Join-Path (Split-Path -Parent $nodeExecutable) 'LICENSE'
if (Test-Path -LiteralPath $nodeLicense) { Copy-Item -LiteralPath $nodeLicense -Destination (Join-Path $noticeRoot 'NODE-LICENSE.txt') -Force }
else {
    $nodeVersion = (& $nodeExecutable --version).Trim()
    if ($nodeVersion -notmatch '^v\d+\.\d+\.\d+$') { throw 'Unexpected Node runtime version' }
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nodejs/node/$nodeVersion/LICENSE" -OutFile (Join-Path $noticeRoot 'NODE-LICENSE.txt')
}
$manifest = @{
    version = '0.3.0'; built_utc = [DateTime]::UtcNow.ToString('o'); node_version = (& $nodeExecutable --version).Trim()
    architecture = 'win-x64'; orca_modified = $false; hardware_verified = $false
    files = @(Get-ChildItem -LiteralPath $outputRoot -File -Recurse | Where-Object { $_.Name -ne 'build-manifest.json' } | ForEach-Object {
        @{path=$_.FullName.Substring($outputRoot.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputRoot 'build-manifest.json') -Encoding utf8
$archive = Join-Path (Split-Path -Parent $outputRoot) ((Split-Path -Leaf $outputRoot) + '-win-x64.zip')
Compress-Archive -LiteralPath $outputRoot -DestinationPath $archive -Force
Write-Output "Portable package: $archive"
