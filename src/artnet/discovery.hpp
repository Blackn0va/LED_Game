// Art-Net-Knoten im LAN finden: ArtPoll als Broadcast, ArtPollReply auf Port 6454.
//
// Die Antworten gehen laut Art-Net 4 an Port 6454 des Fragenden – deshalb bindet die Suche
// genau diesen Port. Läuft schon ein Art-Net-Programm (Jinx, xLights, QLC+, Resolume), ist
// er belegt; dann hilft nur --ip.
//
// Threads: frei (jeder Aufruf hat seinen Socket); NetworkSession muss bestehen.
// Fehler: browse wirft Error(network) bei belegtem Port 6454 oder Winsock-Fehlern.

#pragma once

#include "artnet/protocol.hpp"

#include <chrono>
#include <vector>

namespace ldu::artnet {

std::vector<ArtNode> browse(std::chrono::milliseconds wait);

}  // namespace ldu::artnet
