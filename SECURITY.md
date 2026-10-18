# Sicherheit

LichtDuell spricht nur mit Geräten im eigenen LAN (UDP, ein HTTP-GET an WLED) und nimmt keine Verbindungen von außen an. Angriffsfläche sind Antworten aus dem LAN, die das Programm liest: Govee-Suche, mDNS, ArtPollReply, WLED `/json/info`. Diese Leser begrenzen Längen, Schachtelungstiefe und Sprungketten und entfernen Steuerzeichen vor der Konsolenausgabe.

## Lücken melden

Bitte **nicht** als öffentliches Issue. Stattdessen über „Report a vulnerability“ (private Sicherheitsmeldung) im Reiter *Security* des Repositorys. Hilfreich: betroffene Version oder Commit, ein Paket bzw. eine Antwort, die das Problem auslöst, und was passiert (Absturz, Hänger, falsche Ausgabe).

## Unterstützte Fassungen

Nur der jeweils neueste Stand von `main`.
