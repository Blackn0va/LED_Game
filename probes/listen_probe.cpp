// Lausch-Probe: Was sendet das Spiel beim Beenden noch – ohne Streifen?
//
// Lauscht auf 127.0.0.1:4003 und protokolliert jede Govee-Nachricht mit Zeit seit dem
// Start. Bilder werden nur gezählt, alles andere (Razer an/aus, turn, devStatus, scan)
// steht einzeln da. Damit lässt sich prüfen, ob die Aus-Folge auch beim Schließen des
// Fensters noch rausgeht: LichtDuell --testbild --ip 127.0.0.1 starten und beenden.
//
//   ldu_probe_listen <sekunden>

#include "govee/protocol.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void say(const std::string& line) {
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        say("Aufruf: ldu_probe_listen <sekunden>");
        return 2;
    }
    const int seconds = std::atoi(argv[1]);
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return 1;
    }
    const SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(ldu::govee::kCommandPort);
    inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);
    if (bind(s, reinterpret_cast<const sockaddr*>(&local), static_cast<int>(sizeof(local))) == SOCKET_ERROR) {
        say("Port 4003 auf 127.0.0.1 nicht frei (" + std::to_string(WSAGetLastError()) + ")");
        return 1;
    }
    const std::string on = ldu::govee::razer_message(ldu::govee::razer_mode_packet(true));
    const std::string off = ldu::govee::razer_message(ldu::govee::razer_mode_packet(false));
    const auto t0 = std::chrono::steady_clock::now();
    const auto end = t0 + std::chrono::seconds(seconds);
    long frames = 0;
    long long last_frame_ms = -1;
    say("lausche " + std::to_string(seconds) + " s auf 127.0.0.1:4003");
    while (std::chrono::steady_clock::now() < end) {
        WSAPOLLFD pfd{};
        pfd.fd = s;
        pfd.events = POLLRDNORM;
        if (WSAPoll(&pfd, 1, 200) <= 0) {
            continue;
        }
        char buf[2048];
        const int n = recv(s, buf, static_cast<int>(sizeof(buf)), 0);
        if (n <= 0) {
            continue;
        }
        const std::string msg(buf, static_cast<std::size_t>(n));
        const long long ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        if (msg.find(R"("cmd":"razer")") != std::string::npos && msg != on && msg != off) {
            ++frames;
            last_frame_ms = ms;
            continue;
        }
        const std::string what = msg == on ? "Razer AN" : (msg == off ? "Razer AUS" : msg);
        say(std::to_string(ms) + " ms  " + what + "   (Bilder bisher " + std::to_string(frames) +
            ", letztes bei " + std::to_string(last_frame_ms) + " ms)");
    }
    say("Ende: " + std::to_string(frames) + " Bilder, letztes bei " + std::to_string(last_frame_ms) + " ms");
    closesocket(s);
    WSACleanup();
    return 0;
}
