// Ausgabe eines Spielbilds auf einen Govee-Streifen über den Razer-Modus.
//
// Start: Gerät einschalten („turn“), Razer-Modus an. Danach Bilder über FramePacer.
// push() schaltet den Razer-Modus alle kRearmS Sekunden erneut ein (DMXCore macht das
// ebenso): Fällt das Gerät aus dem Modus, weil jemand die App bedient oder das WLAN
// kurz weg war, kommt das Spielbild von selbst zurück.
//
// Ende: 2 s Ruhe nach dem letzten Bild, dann den Razer-Modus DREIMAL aus. Gemessen
// 2026-10-18 am H612C (Nutzer schaute hin):
//   1× Aus direkt nach dem letzten Bild           → blieb minutenlang auf dem Bild stehen,
//                                                   auch das laut OpenRGB erwartete
//                                                   Zurückfallen nach einer Minute kam nicht
//   3× Aus nach 0,15 s (Fenster geschlossen)      → blieb stehen
//   3× Aus nach 2 s                               → zurück zum Zustand davor (Tageslicht)
//   3× Aus nach Minuten                           → zurück zur Szene
// Kürzere Pausen als 2 s sind nicht als sicher belegt. Wer die Ruhe kürzt, prüft am
// Streifen nach. War der Streifen vor dem Spiel aus (devStatus, siehe query_status), geht
// er danach wieder aus; ohne bekannten Zustand bleibt er an.
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: Konstruktor wirft Error(network) bei ungültiger IP, push() wirft
// Error(protocol) bei mehr als kMaxRazerColors Zellen. Sendefehler werfen nicht, sie
// zählen in status() (fail-open: ein verlorenes Bild beendet kein Spiel).

#pragma once

#include "core/rgb.hpp"
#include "govee/frame_pacer.hpp"
#include "govee/udp.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ldu::govee {

inline constexpr double kKeepaliveS = 2.0;
inline constexpr double kRearmS = 2.0;

// Wie das Spielbild auf den physischen Streifen fällt, vom Kanonen-Ende aus:
//   [ skip dunkel ][ Kampffeld ][ zone Effektbereich ]
//   segments  Farben im Razer-Paket; 0 = so viele wie Spielzellen (alter Weg, 1:1).
//             Der H612C nimmt höchstens 40 an (gemessen 2026-10-18: 30/40 ja, 50–80 nicht).
//   skip      Einheiten am Kanonen-Ende, die dunkel bleiben – beim Nutzer die Strecke
//             hinter dem Bett und die Wand hoch bis zur Decke.
//   zone      Einheiten am Computer-Ende für den Effektbereich (Game::render_zone) – beim
//             Nutzer das Stück hinter der rechten Zimmerecke bis zu den Waben.
// Die Spielzellen werden gleichmäßig auf segments - skip - zone Einheiten verteilt.
struct StripLayout final {
    int segments = 0;
    int skip = 0;
    int zone = 0;
};

struct StripSettings final {
    std::string ip;
    std::string label;          // Anzeige, z. B. das Modell aus der Suche
    double max_fps = 30.0;
    bool reversed = false;      // Kanone am Ende statt am Anfang des Streifens
    double brightness = 1.0;    // 0..1, in Software auf die Farben gerechnet
    bool turn_off_at_end = false;  // war vor dem Spiel aus → beim Beenden wieder aus
    StripLayout layout;
    // Beenden: erst stop_settle_s Ruhe nach dem letzten Bild, dann stop_repeats-mal Aus im
    // Abstand stop_gap_s (Messung siehe Kopf). Tests setzen die Zeiten auf 0.
    double stop_settle_s = 2.0;
    int stop_repeats = 3;
    double stop_gap_s = 0.1;
};

// Spielbild → Streifenbild: auf das Layout verteilt (Versatz dunkel), Helligkeit
// skaliert, optional gespiegelt. Gespiegelt wird zuletzt – der Versatz bleibt so immer am
// Kanonen-Ende, auch mit --umgedreht.
std::vector<Rgb> prepare_led_frame(std::span<const Rgb> field, std::span<const Rgb> zone, const StripLayout& layout,
                                   bool reversed, double brightness);
// Ohne Effektbild: der Effektbereich bleibt dunkel.
std::vector<Rgb> prepare_led_frame(std::span<const Rgb> frame, const StripLayout& layout, bool reversed,
                                   double brightness);

class StripOutput final {
public:
    // Sendet per UDP an settings.ip.
    explicit StripOutput(StripSettings settings);
    // Sendet in eine beliebige Senke (Tests).
    StripOutput(StripSettings settings, std::unique_ptr<PacketSink> sink);
    ~StripOutput();
    StripOutput(const StripOutput&) = delete;
    StripOutput& operator=(const StripOutput&) = delete;

    void start(double now_s);
    void push(std::span<const Rgb> field, std::span<const Rgb> zone, double now_s);
    void push(std::span<const Rgb> frame, double now_s);
    void stop();

    // Eine Zeile für die Konsole: Ziel, Bildrate, gesendete Bilder, letzte Störung.
    std::string status() const;

private:
    void send(const std::string& payload);

    StripSettings settings_;
    std::unique_ptr<PacketSink> sink_;
    FramePacer pacer_;
    std::vector<Rgb> last_sent_;
    double last_mode_s_ = 0.0;
    std::uint64_t frames_ = 0;
    std::uint64_t failures_ = 0;
    std::string last_error_;
    bool running_ = false;
};

}  // namespace ldu::govee
