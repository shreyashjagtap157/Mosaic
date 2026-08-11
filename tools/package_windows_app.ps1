param(
    [string]$Config = "Release",
    [string]$BuildDir = "build\cmake",
    [string]$StageDir = "dist\windows\stage",
    [string]$InstallerDir = "dist\windows"
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
New-Item -ItemType Directory -Force -Path $InstallerDir | Out-Null

cmake --build $BuildDir --config $Config --target mosaic_shared mosaic-desktop
cmake --install $BuildDir --config $Config --prefix $StageDir

$wix = Get-Command candle -ErrorAction SilentlyContinue
if (-not $wix) {
    Write-Host "WiX tools not found. Staged install tree is ready at $StageDir."
    exit 0
}

Write-Host "WiX tools detected. MSI packaging can be wired from $StageDir using packaging\windows\MosaicDesktop.wxs."
