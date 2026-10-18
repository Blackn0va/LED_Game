#include "devices/connect.hpp"

#include "artnet/discovery.hpp"
#include "artnet/driver.hpp"
#include "core/error.hpp"
#include "govee/discovery.hpp"
#include "govee/driver.hpp"
#include "net/udp.hpp"
#include "sacn/driver.hpp"
#include "sacn/protocol.hpp"
#include "sacn/router.hpp"
#include "wled/discovery.hpp"
#include "wled/driver.hpp"

#include <exception>
#include <thread>
#include <utility>

namespace ldu::devices {
namespace {

using namespace std::chrono_literals;
using lights::DeviceType;

inline constexpr auto kSearchWait = 2000ms;
inline constexpr auto kStatusWait = 700ms;
inline constexpr auto kInfoWait = 1500ms;

std::string pad(std::string s, std::size_t width) {
    if (s.size() < width) {
        s.append(width - s.size(), ' ');
    }
    return s;
}

std::vector<Found> search(DeviceType type, std::chrono::milliseconds wait) {
    std::vector<Found> out;
    switch (type) {
    case DeviceType::govee:
        for (const auto& d : govee::scan_devices(wait)) {
            Found f;
            f.type = type;
            f.ip = d.ip;
            f.name = d.sku;
            f.detail = d.device.empty() ? "" : "Geräte-ID " + d.device;
            out.push_back(std::move(f));
        }
        break;
    case DeviceType::wled:
        for (const auto& d : wled::browse(wait)) {
            Found f;
            f.type = type;
            f.ip = d.ip;
            f.name = d.info && !d.info->name.empty() ? d.info->name : d.instance;
            if (d.info) {
                f.leds = d.info->leds;
                f.detail = std::to_string(d.info->leds) + " LEDs";
                if (!d.info->version.empty()) {
                    f.detail += " · " + d.info->version;
                }
            } else {
                f.detail = "/json/info antwortet nicht";
            }
            out.push_back(std::move(f));
        }
        break;
    case DeviceType::artnet:
        for (const auto& n : artnet::browse(wait)) {
            Found f;
            f.type = type;
            f.ip = n.ip;
            f.name = n.short_name.empty() ? n.long_name : n.short_name;
            f.universe = n.port_address;
            if (n.port_address) {
                f.detail = "Universum " + std::to_string(*n.port_address);
            }
            out.push_back(std::move(f));
        }
        break;
    case DeviceType::sacn:
        break;  // E1.31-Empfänger melden sich nicht (siehe connect.hpp)
    }
    return out;
}

std::string many_devices(DeviceType type, const std::vector<Found>& found) {
    std::string text =
        "Mehrere " + std::string(lights::device_name(type)) + "-Geräte gefunden – bitte eins mit --ip wählen:\n";
    for (const Found& f : found) {
        text += "  " + found_line(f) + "\n";
    }
    return text;
}

Found pick_single(DeviceType type, const Say& say) {
    say("Suche " + std::string(lights::device_name(type)) + " im Netz (2 s) …\n");
    const auto found = search(type, kSearchWait);
    if (found.empty()) {
        throw Error(Error::Kind::network, no_device_hint(type));
    }
    if (found.size() > 1) {
        throw Error(Error::Kind::usage, many_devices(type, found));
    }
    say("Gefunden:\n  " + found_line(found.front()) + "\n");
    return found.front();
}

}  // namespace

Report discover(DeviceType type, std::chrono::milliseconds wait) {
    Report r;
    try {
        r.found = search(type, wait);
    } catch (const std::exception& e) {
        // Grenze je Suche und Thread (discover_all): Scheitert eine (Port belegt), steht der
        // Grund in der Liste und die übrigen Geräte bleiben wählbar (bewusst fail-open, Komfort).
        r.problems.push_back(std::string(lights::device_name(type)) + ": " + e.what());
    }
    return r;
}

Report discover_all(std::chrono::milliseconds wait) {
    const std::vector<DeviceType> types{DeviceType::govee, DeviceType::wled, DeviceType::artnet};
    std::vector<Report> parts(types.size());
    std::vector<std::thread> threads;
    threads.reserve(types.size());
    for (std::size_t i = 0; i < types.size(); ++i) {
        // discover() fängt jede Ausnahme selbst – aus dem Thread kann nichts entweichen.
        threads.emplace_back([&parts, &types, i, wait] { parts[i] = discover(types[i], wait); });
    }
    for (std::thread& t : threads) {
        t.join();
    }
    Report all;
    for (Report& p : parts) {
        for (Found& f : p.found) {
            all.found.push_back(std::move(f));
        }
        for (std::string& s : p.problems) {
            all.problems.push_back(std::move(s));
        }
    }
    return all;
}

std::string complete_manual(Found& f) {
    if (f.type != DeviceType::wled || f.ip.empty()) {
        return {};
    }
    std::string why;
    const auto info = wled::fetch_info(f.ip, kInfoWait, why);
    if (!info) {
        return "WLED antwortet nicht auf /json/info (" + why + ") – Abschnitte bitte mit --segmente setzen.";
    }
    f.name = info->name;
    f.leds = info->leds;
    f.detail = std::to_string(info->leds) + " LEDs";
    return {};
}

std::string found_line(const Found& f) {
    std::string line = pad(std::string(lights::device_name(f.type)), 9) + pad(f.ip, 17);
    line += f.name.empty() ? "?" : f.name;
    if (!f.detail.empty()) {
        line += " · " + f.detail;
    }
    return line;
}

std::string no_device_hint(DeviceType type) {
    switch (type) {
    case DeviceType::govee:
        return "Kein Govee-Gerät hat auf die Suche geantwortet. Prüfen:\n"
               "  1. Govee-App → Streifen → Einstellungen → „LAN-Steuerung“ einschalten\n"
               "     (der Schalter erscheint manchmal erst nach einigen Minuten).\n"
               "  2. PC und Streifen im selben Netz (kein Gast-WLAN, keine Client-Isolation).\n"
               "  3. Windows-Firewall: eingehend UDP 4002 für LichtDuell.exe erlauben.\n"
               "  4. Oder die IP direkt angeben (steht im Router): LichtDuell --ip 192.168.x.y\n"
               "Ohne Streifen spielen: LichtDuell --nur-vorschau";
    case DeviceType::wled:
        return "Kein WLED-Gerät hat auf die mDNS-Suche geantwortet. Prüfen:\n"
               "  1. PC und WLED im selben Netz (kein Gast-WLAN, keine Client-Isolation).\n"
               "  2. Windows-Firewall: eingehend UDP für LichtDuell.exe erlauben.\n"
               "  3. Oder die IP direkt angeben (WLED-App oder Router): LichtDuell --gerät wled --ip 192.168.x.y\n"
               "Ohne Streifen spielen: LichtDuell --nur-vorschau";
    case DeviceType::artnet:
        return "Kein Art-Net-Knoten hat auf ArtPoll geantwortet. Prüfen:\n"
               "  1. PC und Knoten im selben Netz; Art-Net im Knoten eingeschaltet.\n"
               "  2. Windows-Firewall: eingehend UDP 6454 für LichtDuell.exe erlauben.\n"
               "  3. Oder die IP direkt angeben: LichtDuell --gerät artnet --ip 192.168.x.y --universum 0\n"
               "Ohne Streifen spielen: LichtDuell --nur-vorschau";
    case DeviceType::sacn:
        return "sACN braucht keine Suche: ohne --ip geht das Bild per Multicast an das Universum.";
    }
    return {};
}

Target resolve(const Request& request, const Say& say) {
    Target t;
    t.type = request.type;
    t.ip = request.ip;
    t.universe = request.universe.value_or(lights::default_universe(request.type));
    switch (request.type) {
    case DeviceType::govee: {
        if (t.ip.empty()) {
            const Found f = pick_single(t.type, say);
            t.ip = f.ip;
            t.label = f.name;
        }
        // Zustand vor dem Spiel: War der Streifen aus, geht er beim Beenden wieder aus.
        // Ohne Antwort bleibt er an (fail-open, Komfort – siehe query_status).
        std::string why;
        const auto before = govee::query_status(t.ip, kStatusWait, why);
        t.turn_off_at_end = before && !before->on;
        if (!before) {
            t.note = " · Zustand unbekannt (" + why + "), bleibt danach an";
        } else if (!before->on) {
            t.note = " · war aus, geht danach wieder aus";
        }
        break;
    }
    case DeviceType::wled: {
        if (t.ip.empty()) {
            const Found f = pick_single(t.type, say);
            t.ip = f.ip;
            t.label = f.name;
        }
        std::string why;
        if (const auto info = wled::fetch_info(t.ip, kInfoWait, why)) {
            t.label = info->name;
            t.leds = info->leds;
            t.port = info->udp_port;
        } else {
            t.note = " · /json/info: " + why;
        }
        break;
    }
    case DeviceType::artnet:
        if (t.ip.empty()) {
            const Found f = pick_single(t.type, say);
            t.ip = f.ip;
            t.label = f.name;
            if (!request.universe && f.universe) {
                t.universe = *f.universe;  // Universum des ersten Ausgangs, wie der Knoten es meldet
            }
        }
        break;
    case DeviceType::sacn:
        break;  // ohne IP: Multicast (open_output)
    }
    return t;
}

std::unique_ptr<lights::LightOutput> open_output(const Target& t, lights::OutputSettings settings) {
    std::unique_ptr<lights::LightProtocol> protocol;
    std::string ip = t.ip;
    std::uint16_t port = 0;
    switch (t.type) {
    case DeviceType::govee: {
        govee::GoveeOptions g;
        g.turn_off_at_end = t.turn_off_at_end;
        protocol = std::make_unique<govee::GoveeProtocol>(g);
        port = govee::kCommandPort;
        break;
    }
    case DeviceType::wled:
        protocol = std::make_unique<wled::WledProtocol>();
        port = t.port > 0 ? static_cast<std::uint16_t>(t.port) : wled::kUdpPort;
        break;
    case DeviceType::artnet:
        protocol = std::make_unique<artnet::ArtNetProtocol>(t.universe);
        port = artnet::kPort;
        break;
    case DeviceType::sacn:
        protocol = std::make_unique<sacn::SacnProtocol>(t.universe, sacn::random_cid());
        port = sacn::kPort;
        break;
    }
    // Multicast (sACN ohne IP): je Universum die eigene Gruppe (sacn/router.hpp).
    const bool multicast = t.type == DeviceType::sacn && ip.empty();
    if (ip.empty() && !multicast) {
        throw Error(Error::Kind::usage, std::string(lights::device_name(t.type)) + " braucht eine IP (--ip).");
    }
    settings.target = multicast ? "Multicast " + sacn::multicast_group(t.universe) : ip;
    if (lights::uses_universe(t.type)) {
        // Im Testbild ist das Layout 1:1 (segments 0) – dann ist die Zahl der Universen erst
        // beim ersten Bild bekannt und die Zeile nennt nur das erste.
        const int count = lights::universes_needed(settings.layout.segments);
        settings.target += count > 1 ? " · Universen " + std::to_string(t.universe) + "–" +
                                           std::to_string(t.universe + count - 1)
                                     : " · Universum " + std::to_string(t.universe);
    }
    if (settings.label.empty()) {
        settings.label = t.label;
    }
    std::unique_ptr<net::PacketSink> sink;
    if (multicast) {
        sink = std::make_unique<sacn::UniverseRouter>(
            [](const std::string& group) { return net::UdpSender::open(group, sacn::kPort); });
    } else {
        sink = net::UdpSender::open(ip, port);
    }
    return std::make_unique<lights::LightOutput>(std::move(settings), std::move(protocol), std::move(sink));
}

}  // namespace ldu::devices
