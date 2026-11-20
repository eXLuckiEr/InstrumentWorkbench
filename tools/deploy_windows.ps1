<#
.SYNOPSIS
  Packs a Windows build of IWP into dist\IWP-win64-<version>.zip (Qt DLLs, plugins, licences).

.DESCRIPTION
  Run it from anywhere after building with an MSVC kit:
      pwsh tools\deploy_windows.ps1 -Config Release -Version 0.1.0
  windeployqt is given the host, the SDK, the helper AND every plugin: the plugins are loaded by the
  host at run time, so without this the Qt modules only they use (Network, SerialPort, WebEngine...)
  would not be copied. Needs Qt's bin directory in PATH (or QT_ROOT_DIR), and the MSVC environment
  (vcvars / "x64 Native Tools" prompt) if the Visual C++ runtime DLLs are to be bundled too.
  QtKeychain (qt6keychain.dll) is copied from -QtKeychainRoot (default: $env:QTKEYCHAIN_ROOT).
#>
param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$OutDir = "dist",
    [string]$Version = "dev",
    [string]$QtKeychainRoot = $env:QTKEYCHAIN_ROOT
)
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$bin = Join-Path $BuildDir "bin\$Config"
if (-not (Test-Path (Join-Path $bin "iwp_host.exe"))) {
    throw "iwp_host.exe not found in ${bin}: build the project first (cmake --build $BuildDir --config $Config)"
}

$wd = (Get-Command windeployqt -ErrorAction SilentlyContinue).Source
if (-not $wd -and $env:QT_ROOT_DIR) {
    $candidate = Join-Path $env:QT_ROOT_DIR "bin\windeployqt.exe"
    if (Test-Path $candidate) { $wd = $candidate }
}
if (-not $wd) { throw "windeployqt not found: add Qt's bin directory to PATH or set QT_ROOT_DIR" }

$name = "IWP-win64-$Version"
$app = Join-Path $OutDir $name
if (Test-Path $app) { Remove-Item -Recurse -Force $app }
New-Item -ItemType Directory -Force -Path $app | Out-Null
Copy-Item -Path (Join-Path $bin "*") -Destination $app -Recurse
Get-ChildItem $app -Recurse -Include *.pdb, *.ilk, *.exp, *.lib | Remove-Item -Force

$targets = @("iwp_host.exe", "iwp_sdk.dll", "iwp_askpass.exe") | ForEach-Object { Join-Path $app $_ }
$pluginDir = Join-Path $app "plugins"
if (Test-Path $pluginDir) {
    $targets += (Get-ChildItem $pluginDir -Filter *.dll | ForEach-Object { $_.FullName })
}
# QtKeychain
$kcDll = $null
if ($QtKeychainRoot) {
    $kcDll = Get-ChildItem -Path $QtKeychainRoot -Recurse -Filter "qt6keychain.dll" -ErrorAction SilentlyContinue |
             Select-Object -First 1
    if (-not $kcDll) { throw "qt6keychain.dll not found under $QtKeychainRoot" }
} else {
    Write-Warning "QtKeychain not given: if the host was built with it, the program will not start without qt6keychain.dll"
}

# windeployqt takes qt6keychain.dll for a Qt module (the name starts with "qt6") and looks for it in Qt's bin.
# So it is put there for the time of the run and removed afterwards.
$qtBin = Split-Path -Parent $wd
$tmpKc = $null
if ($kcDll -and -not (Test-Path (Join-Path $qtBin "qt6keychain.dll"))) {
    $tmpKc = Join-Path $qtBin "qt6keychain.dll"
    Copy-Item $kcDll.FullName $tmpKc
}
try {
    Write-Host "windeployqt: $wd"
    & $wd --release --no-opengl-sw --dir $app @targets
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }
} finally {
    if ($tmpKc) { Remove-Item -Force $tmpKc -ErrorAction SilentlyContinue }
}

if ($kcDll) { Copy-Item $kcDll.FullName $app -Force }

Copy-Item LICENSE, README.md $app
Copy-Item -Recurse THIRD_PARTY (Join-Path $app "THIRD_PARTY")

$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $app -DestinationPath $zip
Write-Host "Done: $zip"
