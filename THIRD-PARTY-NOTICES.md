# Fremdkomponenten

| Komponente | Stand | Lizenz | Enthalten in |
|---|---|---|---|
| doctest | 2.4.11 (`ae7a1353`) | MIT | nur Test-Programme, nicht in `LichtDuell.exe` |

Lizenztext: [`licenses/doctest/LICENSE.txt`](licenses/doctest/LICENSE.txt).

`LichtDuell.exe` selbst nutzt nur das Windows-SDK (Winsock, IP Helper, Konsole,
`winmm` für Zeitgeber und Ton, `shell32`/`ole32` für den Ordner der Bestenliste) und die
Visual-C++-Laufzeit. Alle Klänge erzeugt das Programm selbst; es liegen keine fremden
Ton- oder Bilddateien bei.

Die Gerätetreiber enthalten keinen fremden Code. Sie sind nach öffentlichen
Beschreibungen geschrieben:

| Protokoll | Quelle |
|---|---|
| Govee LAN-API | Govee-WLAN-Anleitung; Razer-Paketaufbau nach OpenRGB-MR 2172 (nur gelesen, nichts übernommen) |
| WLED UDP-Echtzeit, JSON-API | WLED-Doku (kno.wled.ge) |
| Art-Net 4 | Spezifikation von Artistic Licence. Vermerk, um den der Rechteinhaber bei Art-Net-Produkten bittet (Wortlaut aus der Erinnerung): „Art-Net™ Designed by and Copyright Artistic Licence“ |
| sACN | ANSI E1.31 (ESTA) |
| mDNS / DNS-SD | RFC 6762, RFC 6763, RFC 1035 |
