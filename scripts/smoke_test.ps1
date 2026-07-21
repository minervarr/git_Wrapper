$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$exe  = Join-Path $root 'build\windows\git_wrapper.exe'

Write-Host '[1/2] Building...' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'build_msvc.ps1')

Write-Host '[2/2] Running smoke test (git_wrapper help)...' -ForegroundColor Cyan
& $exe help | Out-Null
$rc = $LASTEXITCODE

if ($rc -eq 0) {
    Write-Host "PASS: git_wrapper help exited 0" -ForegroundColor Green
    exit 0
} else {
    Write-Host "FAIL: git_wrapper help exited $rc" -ForegroundColor Red
    exit 1
}
