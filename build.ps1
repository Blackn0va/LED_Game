#Requires -Version 7
<#
.SYNOPSIS
    Bau von LichtDuell in Stufen (Muster Signierungstool\build.ps1, ohne Setup und Signatur).

.DESCRIPTION
    Stufen (in dieser Reihenfolge, -Stage all = alle):
      check      vendorte Dateien gegen deps.lock.json
      configure  cmake -S . -B out\rel (CMake prüft die Hashes ein zweites Mal)
      build      cmake --build (Release)
      test       ctest -L unit

    WICHTIG: Keine Stufen stage/sign/installer – das Spiel wird nicht ausgeliefert. Wer
    es weitergeben will, ergänzt sie nach dem Muster aus Signierungstool\build.ps1.

    Voraussetzungen: Build Tools 2022 (MSVC 14.44), CMake 3.30+ (gemessen 4.4.4).
    Ergebnis: out\rel\bin\Release\LichtDuell.exe

    Diese Datei ist UTF-8 MIT BOM: Windows PowerShell 5.1 liest Dateien ohne BOM als
    ANSI, die Umlaute der Meldungen würden zu „Ã¤“. Mit BOM scheitert ein versehentlicher
    Start unter 5.1 lesbar an #Requires, statt Unsinn zu drucken.

.EXAMPLE
    pwsh -File build.ps1                   # alles
    pwsh -File build.ps1 -Stage build      # nur bis einschließlich build
    pwsh -File build.ps1 -Clean            # Bauordner vorher löschen
#>
[CmdletBinding()]
param(
    # Letzte Stufe, die laufen soll.
    [ValidateSet('check', 'configure', 'build', 'test', 'all')]
    [string]$Stage = 'all',
    # Bauordner relativ zum Repo.
    [string]$BuildDir = 'out\rel',
    # Bauordner vor configure löschen.
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

$root = $PSScriptRoot
$lock = Get-Content -LiteralPath (Join-Path $root 'deps.lock.json') -Raw -Encoding utf8 | ConvertFrom-Json
$order = @('check', 'configure', 'build', 'test')
$last = if ($Stage -eq 'all') { $order.Count - 1 } else { [array]::IndexOf($order, $Stage) }
$build = Join-Path $root $BuildDir

function Step([string]$Name) {
    Write-Host ""
    Write-Host "==> $Name" -ForegroundColor Cyan
}

function Fail([string]$Message) {
    Write-Host "FEHLER: $Message" -ForegroundColor Red
    throw "build.ps1: $Message"
}

# Natives Programm aufrufen und den Exitcode prüfen. FALLE: $LASTEXITCODE gibt es in
# einer frischen Konsole unter StrictMode erst nach dem ersten nativen Aufruf – deshalb
# vorher auf 0 setzen und erst danach lesen.
function Invoke-Native([string]$What, [scriptblock]$Block) {
    $global:LASTEXITCODE = 0
    & $Block
    $code = $global:LASTEXITCODE
    if ($code -ne 0) { Fail "$What (Exit $code)" }
}

function Get-ProjectVersion {
    $m = Select-String -LiteralPath (Join-Path $root 'CMakeLists.txt') -Pattern 'project\(LichtDuell VERSION (\d+\.\d+\.\d+)'
    if (-not $m) { Fail 'Version in CMakeLists.txt nicht gefunden' }
    return $m.Matches[0].Groups[1].Value
}

Write-Host "LichtDuell $(Get-ProjectVersion) – Stufen bis '$($order[$last])'"

for ($i = 0; $i -le $last; $i++) {
    switch ($order[$i]) {
        'check' {
            Step 'check: Sperrdatei'
            foreach ($rel in $lock.vendored.doctest.sha256.PSObject.Properties.Name) {
                $file = Join-Path $root $rel
                if (-not (Test-Path -LiteralPath $file)) { Fail "Vendorte Datei fehlt: $rel" }
                $h = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($h -ne $lock.vendored.doctest.sha256.$rel) { Fail "Vendorte Datei weicht von deps.lock.json ab: $rel" }
            }
            Write-Host '  ok'
        }
        'configure' {
            Step "configure: $BuildDir"
            if ($Clean -and (Test-Path -LiteralPath $build)) {
                Remove-Item -LiteralPath $build -Recurse -Force
            }
            Invoke-Native 'cmake configure' {
                cmake -S $root -B $build -G $lock.toolchain.generator -A x64 | Out-Host
            }
        }
        'build' {
            Step 'build: Release'
            Invoke-Native 'cmake build' { cmake --build $build --config Release -- /m /v:m | Out-Host }
            $exe = Join-Path $build 'bin\Release\LichtDuell.exe'
            if (-not (Test-Path -LiteralPath $exe)) { Fail "LichtDuell.exe nicht entstanden: $exe" }
            Write-Host "  $exe"
        }
        'test' {
            Step 'test: ctest -L unit'
            Invoke-Native 'ctest' { ctest --test-dir $build -C Release -L unit --output-on-failure | Out-Host }
        }
    }
}

Write-Host ""
Write-Host 'Fertig.' -ForegroundColor Green
