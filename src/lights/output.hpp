// Ausgabe eines Spielbilds an ein LED-Gerät – herstellerunabhängig.
//
// Aufteilung: LightProtocol (ein Treiber je Hersteller) baut nur Datagramme, ohne Netz
// und ohne Uhr. LightOutput macht den Rest, der für alle gleich ist: Layout und
// Helligkeit (lights/layout.hpp), Takt und Haltebild (FramePacer), Senden über eine
// PacketSink, Zählen von Bildern und Fehlern, Abschluss beim Beenden. Vorher steckte
// all das in govee::StripOutput; ein zweiter Hersteller hätte es kopiert.
//
// Ablauf: start() einmal → push() je Spielbild (gedrosselt) → stop() einmal. Der
// Destruktor holt ein vergessenes stop() nach, damit das Gerät nicht im Echtzeitmodus
// hängen bleibt (am Govee H612C blieb es sonst minutenlang auf dem letzten Bild).
//
// Threads: nicht threadsicher; gehört dem Spielthread.
// Fehler: Konstruktor wirft Error(usage), wenn das Layout mehr Farben verlangt als der
// Treiber kann; push() wirft Error(protocol), wenn ein Bild ohne Layout zu groß ist.
// Sendefehler werfen nicht, sie zählen in status() (fail-open: ein verlorenes Bild beendet
// kein Spiel, kein Sicherheitspfad).

#pragma once

#include "core/rgb.hpp"
#include "lights/frame_pacer.hpp"
#include "lights/layout.hpp"
#include "net/udp.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ldu::lights {

// Ein Schritt beim Beenden: erst wait_s warten, dann packet senden.
struct StopStep final {
    double wait_s = 0.0;
    std::string packet;

    friend bool operator==(const StopStep&, const StopStep&) = default;
};

// Ein Gerätetreiber. Bekommt fertige Streifenbilder (genau die Einheiten des Layouts,
// schon gedimmt und gespiegelt) und liefert die Datagramme dafür.
class LightProtocol {
public:
    virtual ~LightProtocol() = default;

    // Für die Statuszeile, z. B. "Govee" oder "WLED".
    virtual std::string name() const = 0;
    // Höchstens so viele Farben je Bild.
    virtual int max_units() const = 0;
    // Ein unverändertes Bild spätestens nach so vielen Sekunden erneut senden.
    virtual double keepalive_s() const = 0;

    // Datagramme beim Start (Govee: einschalten, Razer-Modus an). Darf leer sein.
    virtual std::vector<std::string> start(double now_s) = 0;
    // Datagramme für ein Bild; now_s für zeitgesteuerte Zusätze (Govee: Razer neu an).
    virtual std::vector<std::string> frame(std::span<const Rgb> leds, double now_s) = 0;
    // Abschluss. last ist das zuletzt gesendete Bild (leer, wenn keins ging).
    virtual std::vector<StopStep> stop(std::span<const Rgb> last) = 0;
};

struct OutputSettings final {
    std::string target;         // Ziel für die Statuszeile, z. B. "192.168.1.50"
    std::string label;          // Modell oder Name aus der Suche, darf leer sein
    double max_fps = 30.0;
    bool reversed = false;      // Kanone am Ende statt am Anfang des Streifens
    double brightness = 1.0;    // 0..1, in Software auf die Farben gerechnet
    StripLayout layout;
};

// Wartet die angegebenen Sekunden. Tests geben eine Fassung, die nur mitschreibt.
using Sleeper = std::function<void(double seconds)>;

class LightOutput final {
public:
    // sleeper leer = echtes Warten (Sleep).
    LightOutput(OutputSettings settings, std::unique_ptr<LightProtocol> protocol, std::unique_ptr<net::PacketSink> sink,
                Sleeper sleeper = {});
    ~LightOutput();
    LightOutput(const LightOutput&) = delete;
    LightOutput& operator=(const LightOutput&) = delete;

    void start(double now_s);
    void push(std::span<const Rgb> field, std::span<const Rgb> zone, double now_s);
    void push(std::span<const Rgb> frame, double now_s);
    void stop();

    // Eine Zeile für die Konsole: Gerät, Ziel, Bildrate, gesendete Bilder, letzte Störung.
    std::string status() const;

private:
    void send(const std::string& payload);

    OutputSettings settings_;
    std::unique_ptr<LightProtocol> protocol_;
    std::unique_ptr<net::PacketSink> sink_;
    Sleeper sleeper_;
    FramePacer pacer_;
    std::vector<Rgb> last_sent_;
    std::uint64_t frames_ = 0;
    std::uint64_t failures_ = 0;
    std::string last_error_;
    bool running_ = false;
};

}  // namespace ldu::lights
