# Fremdkomponenten

| Komponente | Stand | Lizenz | Enthalten in |
|---|---|---|---|
| doctest | 2.4.11 (`ae7a1353`) | MIT | nur Test-Programme, nicht in `LichtDuell.exe` |

Lizenztext: [`licenses/doctest/LICENSE.txt`](licenses/doctest/LICENSE.txt).

`LichtDuell.exe` selbst nutzt nur das Windows-SDK (Winsock, IP Helper, Konsole,
`winmm` für Zeitgeber und Ton, `shell32`/`ole32` für den Ordner der Bestenliste) und die
Visual-C++-Laufzeit. Alle Klänge erzeugt das Programm selbst; es liegen keine fremden
Ton- oder Bilddateien bei.
