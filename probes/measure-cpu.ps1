#requires -Version 7.0
<#
.SYNOPSIS
    Misst die CPU-Zeit von LichtDuell und conhost in einem echten Konsolenfenster.

.DESCRIPTION
    Zweck: belegen, wohin die Rechenzeit geht und ob eine Optimierung wirkt (2026-10-18:
    conhost 7,4–8,6 % → 2,2–2,4 % eines Kerns durch 30er-Takt und Zeilenvergleich).
    Startet die EXE in conhost (wie ein Doppelklick), wartet, misst über -Sekunden und
    schließt das Fenster per WM_CLOSE.

    WICHTIG: Öffnet kurz ein sichtbares Konsolenfenster. Mit --nur-vorschau aufrufen, sonst
    steuert das Spiel den Streifen an.

    Voraussetzungen: gebaute LichtDuell.exe (pwsh -File build.ps1).

    FALLE: Direkt nach dem Bau startet die EXE manchmal erst nach der Virenprüfung; das
    Skript wartet 3 s und meldet, wenn es den Prozess nicht findet.

    Diese Datei ist UTF-8 MIT BOM: Windows PowerShell 5.1 läse sie sonst als ANSI und die
    Umlaute der Meldungen würden zu „Ã¤“.

.EXAMPLE
    pwsh -File probes\measure-cpu.ps1 -Arguments '--nur-vorschau --ohne-ton --seed 4 --demo'
#>
[CmdletBinding()]
param(
    # Pfad zur EXE.
    [string]$Exe = (Join-Path $PSScriptRoot '..\out\rel\bin\Release\LichtDuell.exe'),
    # Argumente für LichtDuell.
    [string]$Arguments = '--nur-vorschau --ohne-ton --seed 4 --demo',
    # Messdauer in Sekunden.
    [int]$Seconds = 15
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

. (Join-Path $PSScriptRoot 'console-window.ps1')

$exePath = (Resolve-Path -LiteralPath $Exe).Path
$session = Start-GameWindow -Exe $exePath -Arguments $Arguments
try {
    $game = Get-Process -Id $session.GamePid
    $hostProc = Get-Process -Id $session.ConhostPid
    $cpu0 = $game.TotalProcessorTime.TotalMilliseconds
    $host0 = $hostProc.TotalProcessorTime.TotalMilliseconds
    Start-Sleep -Seconds $Seconds
    $game.Refresh()
    $hostProc.Refresh()
    $cpuGame = $game.TotalProcessorTime.TotalMilliseconds - $cpu0
    $cpuHost = $hostProc.TotalProcessorTime.TotalMilliseconds - $host0
    'Spiel:   {0:N0} ms CPU in {1} s = {2:N1} % eines Kerns' -f $cpuGame, $Seconds, ($cpuGame / ($Seconds * 10))
    'conhost: {0:N0} ms CPU in {1} s = {2:N1} % eines Kerns' -f $cpuHost, $Seconds, ($cpuHost / ($Seconds * 10))
} finally {
    Close-GameWindow -Session $session
}
