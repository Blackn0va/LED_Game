#requires -Version 7.0
<#
.SYNOPSIS
    Gemeinsame Helfer der Fenster-Proben: LichtDuell in conhost starten, Fenster finden,
    schließen. Wird per Dot-Sourcing eingebunden (measure-cpu.ps1, snap-console.ps1).

.DESCRIPTION
    FALLE: Das Konsolenfenster gehört conhost, GetWindowThreadProcessId meldet aber die PID
    des Programms darin (gemessen 2026-10-18). Gesucht wird deshalb nach Fensterklasse
    ConsoleWindowClass UND der PID von LichtDuell.exe. MainWindowHandle von conhost ist 0.

    Diese Datei ist UTF-8 MIT BOM: Windows PowerShell 5.1 läse sie sonst als ANSI.
#>

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class LduConsoleWindow {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    public static IntPtr Find(uint want) {
        IntPtr hit = IntPtr.Zero;
        EnumWindows((h, l) => {
            var c = new System.Text.StringBuilder(256); GetClassNameW(h, c, 256);
            uint pid; GetWindowThreadProcessId(h, out pid);
            if (c.ToString() == "ConsoleWindowClass" && pid == want) { hit = h; }
            return true;
        }, IntPtr.Zero);
        return hit;
    }
}
'@

function Start-GameWindow {
    [CmdletBinding()]
    param([string]$Exe, [string]$Arguments)
    $conhost = Start-Process -FilePath (Join-Path $env:WINDIR 'System32\conhost.exe') `
        -ArgumentList "`"$Exe`" $Arguments" -PassThru
    # WOZU: Nach einem frischen Bau startet die EXE erst nach der Virenprüfung – warten, bis
    # der Prozess da ist, statt fest 2 s anzunehmen.
    $game = $null
    for ($i = 0; $i -lt 50 -and -not $game; $i++) {
        Start-Sleep -Milliseconds 200
        $game = Get-CimInstance Win32_Process -Filter "Name='LichtDuell.exe'" |
            Where-Object { $_.ParentProcessId -eq $conhost.Id } | Select-Object -First 1
    }
    if (-not $game) { throw 'LichtDuell.exe ist nicht gestartet.' }
    $window = [IntPtr]::Zero
    for ($i = 0; $i -lt 50 -and $window -eq [IntPtr]::Zero; $i++) {
        Start-Sleep -Milliseconds 100
        $window = [LduConsoleWindow]::Find([uint32]$game.ProcessId)
    }
    if ($window -eq [IntPtr]::Zero) { throw 'Konsolenfenster nicht gefunden.' }
    Start-Sleep -Seconds 3
    [pscustomobject]@{ ConhostPid = $conhost.Id; GamePid = [int]$game.ProcessId; Window = $window }
}

function Close-GameWindow {
    [CmdletBinding()]
    param($Session)
    # 0x10 = WM_CLOSE: wie ein Klick aufs X – das Spiel bekommt CTRL_CLOSE_EVENT und räumt auf.
    [LduConsoleWindow]::PostMessageW($Session.Window, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
}
