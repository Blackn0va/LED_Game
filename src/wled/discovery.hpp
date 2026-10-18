// WLED-Geräte im LAN finden (mDNS „_wled._tcp.local“) und nach Name und LED-Zahl fragen.
//
// Die IP eines Geräts ist der Absender seiner mDNS-Antwort – SRV- und A-Datensätze
// braucht es dafür nicht. Danach fragt fetch_info jedes Gerät per HTTP nach /json/info.
// Ob WLED auf die Legacy-Unicast-Anfrage antwortet, ist ungeprüft (net/mdns.hpp).
//
// Threads: frei (jeder Aufruf hat seinen Socket); NetworkSession muss bestehen.
// Fehler: browse wirft Error(network), wenn Winsock versagt. fetch_info wirft nicht.

#pragma once

#include "wled/protocol.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace ldu::wled {

struct Found final {
    std::string ip;
    std::string instance;   // mDNS-Name, z. B. "wled-a1b2c3"
    std::optional<WledInfo> info;
};

// Sucht `wait` lang und fragt danach jedes gefundene Gerät nach /json/info.
std::vector<Found> browse(std::chrono::milliseconds wait);

// /json/info eines Geräts. nullopt und Grund, wenn es nicht antwortet oder die Antwort
// kein WLED ist (fail-open: dann gelten die Einstellungen bzw. --segmente).
std::optional<WledInfo> fetch_info(const std::string& ip, std::chrono::milliseconds timeout, std::string& error);

}  // namespace ldu::wled
