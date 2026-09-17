$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

Write-Host "== PSD Explorer: configure =="
cmake -S . -B build-win -A x64 -DPSD_EXPLORER_BUILD_TESTS=ON
Write-Host "== build Release =="
cmake --build build-win --config Release --parallel
Write-Host "== tests =="
ctest --test-dir build-win -C Release --output-on-failure

$dist = Join-Path $root "dist"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item $dist -ItemType Directory | Out-Null
Copy-Item "build-win\Release\PSDExplorerShell.dll" $dist
Copy-Item "build-win\Release\PSDExplorerSetup.exe" $dist
Copy-Item "README.md" $dist
Copy-Item "CHANGES-1.1.2.txt" $dist

$zip = Join-Path $root "PSDExplorer-1.1.2-win-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$dist\*" -DestinationPath $zip
Write-Host "OK: $zip"
