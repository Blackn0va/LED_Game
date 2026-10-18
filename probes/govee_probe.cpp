// Govee-Probe: Was macht der Streifen bei welchem Befehl? Messung am Gerät – jemand muss
// hinschauen; deshalb kein ctest, nur von Hand.
//
//   ldu_probe_govee <ip> status          rohe devStatus-Antwort (ändert nichts am Licht)
//   ldu_probe_govee <ip> razer-aus       B1 00, dreimal im Abstand von 100 ms
//   ldu_probe_govee <ip> neustart        turn 0, 1,5 s warten, turn 1
//   ldu_probe_govee <ip> razer-timeout   Razer an + 2 s Rot, danach NICHTS mehr senden
//   ldu_probe_govee <ip> ende-vergleich  nach 10 s: 3 s Rot → 0,15 s Pause → 3× Aus;
//                                        6 s warten; 3 s Grün → 2 s Pause → 3× Aus.
//                                        Die Farbe, auf der der Streifen stehen bleibt,
//                                        zeigt, welche Pause nicht reicht.
//   ldu_probe_govee <ip> anzahl-test     nach 10 s je 4 s einfarbig mit 30/40/50/60/70/80
//                                        Farben im Paket (Rot/Orange/Gelb/Grün/Blau/Lila),
//                                        dazwischen 1 s dunkel mit 30. Welche Farben
//                                        erscheinen, zeigt, bis zu welcher Zahl das Gerät
//                                        Bilder annimmt. Anlass: 80 Farben → „sehe nichts“.
//
// Anlass (2026-10-18): Nach dem Testbild blieb der H612C trotz B1 00 auf dem letzten Bild
// stehen, statt in seine Szene zurückzukehren.

#include "govee/protocol.hpp"
#include "net/udp.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

void say(const std::string& line) {
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

bool send(ldu::net::UdpSender& sender, const std::string& payload) {
    std::string error;
    if (!sender.send(payload, error)) {
        say("  Sendefehler: " + error);
        return false;
    }
    say("  gesendet: " + payload);
    return true;
}

// Rohantwort auf devStatus: alles, was das Gerät meldet, nicht nur onOff/brightness.
int raw_status(const std::string& ip) {
    const SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        say("Socket-Fehler " + std::to_string(WSAGetLastError()));
        return 1;
    }
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(ldu::govee::kReplyPort);
    if (bind(s, reinterpret_cast<const sockaddr*>(&local), static_cast<int>(sizeof(local))) == SOCKET_ERROR) {
        say("Port 4002 nicht frei (" + std::to_string(WSAGetLastError()) + ")");
        closesocket(s);
        return 1;
    }
    // Erst die Multicast-Suche vom selben Socket: Ohne sie kam auf devStatus allein keine
    // Antwort bei dieser EXE an (2026-10-18), mit LichtDuell --suchen (Suche zuerst) schon.
    sockaddr_in group{};
    group.sin_family = AF_INET;
    group.sin_port = htons(ldu::govee::kScanPort);
    const std::string group_text(ldu::govee::kMulticastGroup);
    inet_pton(AF_INET, group_text.c_str(), &group.sin_addr);
    const std::string scan = ldu::govee::scan_message();
    sendto(s, scan.data(), static_cast<int>(scan.size()), 0, reinterpret_cast<const sockaddr*>(&group),
           static_cast<int>(sizeof(group)));
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(ldu::govee::kCommandPort);
    inet_pton(AF_INET, ip.c_str(), &target.sin_addr);
    const std::string request = ldu::govee::status_message();
    sendto(s, request.data(), static_cast<int>(request.size()), 0, reinterpret_cast<const sockaddr*>(&target),
           static_cast<int>(sizeof(target)));
    WSAPOLLFD pfd{};
    pfd.fd = s;
    pfd.events = POLLRDNORM;
    int rc = 1;
    // Alles ausgeben, was in 2 s ankommt (Scan- und Zustandsantwort), mit Zeit seit dem
    // Senden – das Spiel wartete anfangs nur 700 ms auf devStatus.
    const auto sent = std::chrono::steady_clock::now();
    const auto end = sent + 2s;
    while (std::chrono::steady_clock::now() < end) {
        if (WSAPoll(&pfd, 1, 200) <= 0) {
            continue;
        }
        char buf[2048];
        const int n = recv(s, buf, static_cast<int>(sizeof(buf)), 0);
        if (n > 0) {
            const auto ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - sent).count();
            say(std::to_string(ms) + " ms  " + std::string(buf, static_cast<std::size_t>(n)));
            rc = 0;
        }
    }
    if (rc != 0) {
        say("keine Antwort");
    }
    closesocket(s);
    return rc;
}

// Razer an, 3 s Bilder in `color` mit 30 fps, `pause` Ruhe, dann dreimal Aus (100 ms Abstand).
void stream_then_off(ldu::net::UdpSender& sender, ldu::Rgb color, std::chrono::milliseconds pause,
                     const std::string& name) {
    const auto t0 = std::chrono::steady_clock::now();
    auto stamp = [t0] {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0);
        return std::to_string(ms.count()) + " ms";
    };
    std::string error;
    sender.send(ldu::govee::razer_message(ldu::govee::razer_mode_packet(true)), error);
    const std::vector<ldu::Rgb> frame(30, color);
    const std::string packet = ldu::govee::razer_message(ldu::govee::razer_frame_packet(frame, false));
    for (int i = 0; i < 90; ++i) {
        sender.send(packet, error);
        std::this_thread::sleep_for(33ms);
    }
    say("  " + name + ": letztes Bild bei " + stamp());
    std::this_thread::sleep_for(pause);
    for (int i = 0; i < 3; ++i) {
        sender.send(ldu::govee::razer_message(ldu::govee::razer_mode_packet(false)), error);
        say("  " + name + ": Aus bei " + stamp());
        std::this_thread::sleep_for(100ms);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        say("Aufruf: ldu_probe_govee <ip> status|razer-aus|neustart|razer-timeout");
        return 2;
    }
    const std::string ip = argv[1];
    const std::string step = argv[2];
    const ldu::net::NetworkSession net;
    if (step == "status") {
        return raw_status(ip);
    }
    auto sender = ldu::net::UdpSender::open(ip, ldu::govee::kCommandPort);
    if (step == "razer-aus") {
        for (int i = 0; i < 3; ++i) {
            send(*sender, ldu::govee::razer_message(ldu::govee::razer_mode_packet(false)));
            std::this_thread::sleep_for(100ms);
        }
        return 0;
    }
    if (step == "neustart") {
        send(*sender, ldu::govee::turn_message(false));
        std::this_thread::sleep_for(1500ms);
        send(*sender, ldu::govee::turn_message(true));
        return 0;
    }
    if (step == "razer-timeout") {
        send(*sender, ldu::govee::razer_message(ldu::govee::razer_mode_packet(true)));
        const std::vector<ldu::Rgb> red(30, ldu::Rgb{255, 0, 0});
        for (int i = 0; i < 20; ++i) {
            std::string error;
            sender->send(ldu::govee::razer_message(ldu::govee::razer_frame_packet(red, false)), error);
            std::this_thread::sleep_for(100ms);
        }
        say("  2 s Rot gesendet, ab jetzt nichts mehr. Zeit merken.");
        return 0;
    }
    if (step == "ende-vergleich") {
        say("  Start in 10 s – bitte auf den Streifen schauen.");
        std::this_thread::sleep_for(10s);
        stream_then_off(*sender, ldu::Rgb{255, 0, 0}, 150ms, "Rot");
        std::this_thread::sleep_for(6s);
        stream_then_off(*sender, ldu::Rgb{0, 255, 0}, 2000ms, "Grün");
        say("  fertig.");
        return 0;
    }
    if (step == "anzahl-test") {
        struct Step {
            int count;
            ldu::Rgb color;
            const char* name;
        };
        const Step steps[] = {{30, {255, 0, 0}, "Rot"},   {40, {255, 100, 0}, "Orange"}, {50, {255, 220, 0}, "Gelb"},
                              {60, {0, 255, 0}, "Grün"},  {70, {0, 0, 255}, "Blau"},     {80, {170, 0, 255}, "Lila"}};
        std::string error;
        auto stream = [&](int count, ldu::Rgb color, int frames) {
            const std::vector<ldu::Rgb> frame(static_cast<std::size_t>(count), color);
            const std::string packet = ldu::govee::razer_message(ldu::govee::razer_frame_packet(frame, false));
            for (int i = 0; i < frames; ++i) {
                sender->send(packet, error);
                std::this_thread::sleep_for(50ms);
            }
        };
        say("  Start in 10 s – bitte auf den Streifen schauen.");
        std::this_thread::sleep_for(10s);
        sender->send(ldu::govee::razer_message(ldu::govee::razer_mode_packet(true)), error);
        for (const Step& s : steps) {
            stream(30, ldu::Rgb{0, 0, 0}, 20);  // ~1 s dunkel, sicher angenommen
            say("  " + std::to_string(s.count) + " Farben: " + s.name);
            stream(s.count, s.color, 80);  // ~4 s
        }
        stream(30, ldu::Rgb{0, 0, 0}, 20);
        // Zurück zum Zustand davor: 2 s Ruhe, dreimal Aus (siehe govee/driver.hpp).
        std::this_thread::sleep_for(2s);
        for (int i = 0; i < 3; ++i) {
            sender->send(ldu::govee::razer_message(ldu::govee::razer_mode_packet(false)), error);
            std::this_thread::sleep_for(100ms);
        }
        say("  fertig, Razer aus.");
        return 0;
    }
    say("Unbekannter Schritt: " + step);
    return 2;
}
