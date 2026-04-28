$ErrorActionPreference = 'Stop'

$verFile = Join-Path $PSScriptRoot '..\BSP\fw_build_number.h'

if (-not (Test-Path $verFile)) {
    Write-Host "[Inc_Build] ERROR: $verFile not found"
    exit 1
}

$text = [System.IO.File]::ReadAllText($verFile)

if ($text -notmatch '#define\s+FW_BUILD_NUMBER\s+(\d+)') {
    Write-Host '[Inc_Build] ERROR: FW_BUILD_NUMBER not found in fw_build_number.h'
    exit 1
}

$old = [int]$matches[1]
$new = ($old + 1) -band 0xFF
if ($new -eq 0) { $new = 1 }   # skip 0 on 8-bit wrap so the version never shows as "0.dd.mm.yy"

$updated = [regex]::Replace($text, '(#define\s+FW_BUILD_NUMBER\s+)(\d+)', "`${1}$new")
[System.IO.File]::WriteAllText($verFile, $updated)

Write-Host "[Inc_Build] FW_BUILD_NUMBER: $old -> $new"
