// Govee-Geräte im LAN finden und ihren Zustand abfragen.
//
// Suche per Multicast 239.255.255.250:4001, Antworten an Port 4002 (govee/protocol.hpp).
// Gesendet wird vom selben Socket, der auf 4002 lauscht, weil die Geräte dorthin
// antworten. Socket-Arbeit in net/udp.hpp.
//
// Threads: frei (jeder Aufruf hat seinen Socket); NetworkSession muss bestehen.
// Fehler: scan_devices wirft Error(Kind::network) bei Winsock-Fehlern oder belegtem Port
// 4002. query_status wirft nicht (siehe dort).

#pragma once

#include "govee/protocol.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace ldu::govee {

// Sucht Govee-Geräte mit eingeschalteter LAN-Steuerung und wartet `wait` auf Antworten.
// Wirft, wenn Port 4002 belegt ist (etwa durch Govee Desktop) oder Winsock versagt.
std::vector<DeviceInfo> scan_devices(std::chrono::milliseconds wait);

// Fragt An/Aus und Helligkeit eines Geräts ab. nullopt und Grund in error, wenn es nicht
// antwortet oder Port 4002 belegt ist. Wirft bewusst nicht: Der Zustand entscheidet nur,
// ob der Streifen beim Beenden wieder ausgeht – ohne ihn bleibt er an (fail-open, kein
// Sicherheitspfad). Schickt vorher die Multicast-Suche vom selben Socket: Gemessen
// 2026-10-18 kam bei einer EXE ohne Firewall-Regel auf devStatus allein keine Antwort,
// mit vorheriger Suche schon. Vermutete Ursache: Windows lässt Antworten auf eigenen
// Multicast kurz durch – nicht getrennt geprüft.
std::optional<DeviceStatus> query_status(const std::string& ip, std::chrono::milliseconds wait, std::string& error);

}  // namespace ldu::govee
