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

$iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if (Test-Path $iscc) {
    & $iscc "/O$InstallerDir" "packaging\windows\MosaicDesktop.iss"
    Write-Host "Installer created in $InstallerDir."
    exit 0
}

Write-Host "Inno Setup not found. Staged install tree is ready at $StageDir."
