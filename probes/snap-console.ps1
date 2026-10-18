#requires -Version 7.0
<#
.SYNOPSIS
    Fotografiert LichtDuell in einem klassischen Konsolenfenster (PNG).

.DESCRIPTION
    Zweck: prüfen, ob die Schrift des Fensters alle Zeichen zeichnet – Text auslesen zeigt
    fehlende Glyphen nicht (2026-10-18: ♡ ☆ ▕ ▏ erschienen beim Nutzer als Kästchen).
    Startet die EXE in conhost, wartet -WaitSeconds, fotografiert per PrintWindow und schließt
    das Fenster per WM_CLOSE. Die README-Bilder in docs\bilder stammen von hier.

    WICHTIG: Öffnet kurz ein sichtbares Konsolenfenster. Mit --nur-vorschau aufrufen, sonst
    steuert das Spiel den Streifen an.

    Voraussetzungen: gebaute LichtDuell.exe (pwsh -File build.ps1).

    Diese Datei ist UTF-8 MIT BOM: Windows PowerShell 5.1 läse sie sonst als ANSI und die
    Umlaute der Meldungen würden zu „Ã¤“.

.EXAMPLE
    pwsh -File probes\snap-console.ps1 -Arguments '--nur-vorschau --ohne-ton --auto schnell' -Png out\auto.png -WaitSeconds 12
#>
[CmdletBinding()]
param(
    # Pfad zur EXE.
    [string]$Exe = (Join-Path $PSScriptRoot '..\out\rel\bin\Release\LichtDuell.exe'),
    # Argumente für LichtDuell.
    [string]$Arguments = '--nur-vorschau --ohne-ton --seed 4 --demo',
    # Zieldatei.
    [Parameter(Mandatory)]
    [string]$Png,
    # Wartezeit bis zum Foto.
    [int]$WaitSeconds = 6
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

. (Join-Path $PSScriptRoot 'console-window.ps1')
Add-Type -AssemblyName System.Drawing

$exePath = (Resolve-Path -LiteralPath $Exe).Path
$target = [IO.Path]::GetFullPath($Png)
$session = Start-GameWindow -Exe $exePath -Arguments $Arguments
try {
    Start-Sleep -Seconds ([Math]::Max(0, $WaitSeconds - 3))
    $rect = New-Object LduConsoleWindow+RECT
    [LduConsoleWindow]::GetWindowRect($session.Window, [ref]$rect) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($rect.R - $rect.L), ($rect.B - $rect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    # 2 = PW_RENDERFULLCONTENT: auch Fensterinhalt, der gerade nicht im Vordergrund liegt.
    [LduConsoleWindow]::PrintWindow($session.Window, $hdc, 2) | Out-Null
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    $bmp.Save($target, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    "Bild: $target"
} finally {
    Close-GameWindow -Session $session
}
