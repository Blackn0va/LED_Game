// Entscheidet, wann ein Bild an den Streifen geht.
//
// Das Spiel rechnet mit über 60 Bildern je Sekunde, der Streifen soll höchstens
// max_fps bekommen (DMXCore nennt bis 40/s als Obergrenze, H618A dort flüssig bei 60).
// Unveränderte Bilder gehen nicht raus; nach keepalive_s aber doch, damit das Gerät
// den Razer-Modus nicht nach einer Minute ohne Daten verlässt.
//
// Threads: nicht threadsicher.
// Fehler: keine.

#pragma once

namespace ldu::govee {

class FramePacer final {
public:
    FramePacer(double max_fps, double keepalive_s);

    // true: jetzt senden. now_s ist eine monotone Zeit in Sekunden; changed sagt, ob
    // sich das Bild gegenüber dem zuletzt GESENDETEN unterscheidet.
    bool should_send(double now_s, bool changed) const;
    void mark_sent(double now_s);

private:
    double min_interval_s_;
    double keepalive_s_;
    double last_sent_s_ = 0.0;
    bool sent_any_ = false;
};

}  // namespace ldu::govee
