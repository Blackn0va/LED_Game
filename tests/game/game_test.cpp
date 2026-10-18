// Spiellogik: Treffer, Fehlschuss, Klumpen, Leben, Stufen, Darstellung, Zufall,
// Kombo, Joker, Gold.

#include "core/error.hpp"
#include "game/game.hpp"
#include "game/level.hpp"
#include "game_test_util.hpp"

#include <doctest.h>

#include <vector>

namespace ldu {

using test::enemy_speed;
using test::has_event;
using test::manual;
using test::run;

TEST_CASE("Treffer: gleiche Farbe löst den Punkt auf und gibt Punkte") {
    Game game(manual());
    game.spawn_blob({Color::blue}, 10.0);
    REQUIRE(game.fire(Color::blue));
    run(game, 0.5);
    CHECK(game.blobs().empty());
    CHECK(game.shots().empty());
    CHECK(game.score() == 10 + 50);
    const auto events = game.take_events();
    CHECK(has_event(events, EventKind::hit));
    CHECK(has_event(events, EventKind::cleared));
}

TEST_CASE("Fehlschuss: falsche Farbe steckt sich vorn an und rückt den Klumpen eine Zelle vor") {
    Game game(manual());
    game.spawn_blob({Color::blue}, 10.0);
    REQUIRE(game.fire(Color::red));
    run(game, 0.5);
    REQUIRE(game.blobs().size() == 1);
    const Blob& b = game.blobs().front();
    CHECK(b.colors == std::vector<Color>{Color::red, Color::blue});
    // 0,5 s Drift Richtung Kanone plus eine Zelle für die angesteckte Farbe.
    CHECK(b.head == doctest::Approx(10.0 - 1.0 - enemy_speed(game) * 0.5).epsilon(0.01));
    CHECK(game.score() == 0);
    CHECK(has_event(game.take_events(), EventKind::stuck));
}

TEST_CASE("Klumpen: nach Fehlschüssen müssen die Farben von vorn nach hinten weg") {
    Game game(manual(40));
    game.spawn_blob({Color::blue}, 30.0);
    auto shoot = [&game](Color c) {
        REQUIRE(game.fire(c));
        run(game, 0.5);
    };
    shoot(Color::red);   // [Rot, Blau]
    shoot(Color::blue);  // vorn steht Rot, also steckt Blau an: [Blau, Rot, Blau]
    REQUIRE(game.blobs().size() == 1);
    CHECK(game.blobs().front().colors == std::vector<Color>{Color::blue, Color::red, Color::blue});
    shoot(Color::blue);  // [Rot, Blau]
    shoot(Color::red);   // [Blau]
    REQUIRE(game.blobs().size() == 1);
    CHECK(game.blobs().front().colors == std::vector<Color>{Color::blue});
    shoot(Color::blue);
    CHECK(game.blobs().empty());
}

TEST_CASE("Ziel: ein Schuss trifft nur den vordersten Klumpen") {
    Game game(manual());
    game.spawn_blob({Color::red}, 10.0);
    game.spawn_blob({Color::blue}, 20.0);
    REQUIRE(game.fire(Color::blue));
    run(game, 0.5);
    REQUIRE(game.blobs().size() == 2);
    CHECK(game.blobs()[0].colors == std::vector<Color>{Color::blue, Color::red});
    CHECK(game.blobs()[1].colors == std::vector<Color>{Color::blue});
}

TEST_CASE("Leben: ein Klumpen an der Kanone kostet ein Leben, nach dem dritten ist Schluss") {
    Game game(manual());
    for (int lost = 1; lost <= 3; ++lost) {
        game.spawn_blob({Color::green}, 1.2);
        run(game, 0.5);
        CHECK(game.lives() == 3 - lost);
        CHECK(game.blobs().empty());
    }
    CHECK(game.over());
    CHECK_FALSE(game.fire(Color::red));
    CHECK(has_event(game.take_events(), EventKind::game_over));
}

TEST_CASE("Fehlschuss vor der Kanone: der angesteckte Punkt erreicht sie sofort") {
    Game game(manual());
    game.spawn_blob({Color::red}, 1.5);
    REQUIRE(game.fire(Color::blue));
    run(game, 1.0 / 60.0);
    CHECK(game.lives() == 2);
    CHECK(game.blobs().empty());
}

TEST_CASE("Stufe: nach genug aufgelösten Klumpen steigt die Stufe") {
    Game game(manual());
    const int needed = level_params(1).blobs_to_next;
    for (int i = 0; i < needed; ++i) {
        CHECK(game.level() == 1);
        game.spawn_blob({Color::red}, 15.0);
        REQUIRE(game.fire(Color::red));
        run(game, 0.5);
    }
    CHECK(game.level() == 2);
    CHECK(has_event(game.take_events(), EventKind::level_up));
}

TEST_CASE("Farben: Gelb kommt erst nach dem ersten Endgegner (Stufe 4), Lila ab Stufe 7") {
    Game game(manual());
    CHECK(game.colors_in_play() == 3);
    CHECK_FALSE(game.fire(Color::yellow));
    CHECK(game.fire(Color::red));
    CHECK(first_level_with_colors(4) == 4);
    CHECK(first_level_with_colors(5) == 7);
    // Nie eine neue Farbe mitten in einem Endgegnerkampf.
    for (int l = 3; l <= 30; l += 3) {
        CHECK(level_params(l).colors == level_params(l - 1).colors);
    }
}

TEST_CASE("Kombo: ab fünf Treffern in Folge zählen Treffer doppelt, ein Fehlschuss setzt zurück") {
    Game game(manual(40));
    game.spawn_blob(std::vector<Color>(7, Color::red), 30.0);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(game.fire(Color::red));
        run(game, 0.5);
    }
    CHECK(game.combo() == 5);
    CHECK(game.multiplier() == 2);
    // Fünf Treffer zu je 10 (Kombo 1–4) und einer zu 20 (Kombo 5).
    CHECK(game.score() == 4 * 10 + 20);
    REQUIRE(game.fire(Color::blue));
    run(game, 0.5);
    CHECK(game.combo() == 0);
    CHECK(game.multiplier() == 1);
    CHECK(game.stats().misses == 1);
}

TEST_CASE("Joker: acht Treffer in Folge bringen einen Joker, er passt auf jede Farbe") {
    Game game(manual(40));
    // Bei 20 Zellen Abstand braucht ein Schuss ~0,3 s; nach je 0,4 s ist jeder Treffer
    // aufgelöst. (Bei 30 Zellen waren es ~0,43 s – der achte Schuss flog noch.)
    game.spawn_blob(std::vector<Color>(9, Color::green), 20.0);
    for (int i = 0; i < kComboPerJoker; ++i) {
        REQUIRE(game.fire(Color::green));
        run(game, 0.4);
    }
    CHECK(game.jokers() == 1);
    CHECK(has_event(game.take_events(), EventKind::joker_gained));
    game.spawn_blob({Color::blue}, 5.0);  // davor gesetzt: ist jetzt der vorderste
    REQUIRE(game.fire_joker());
    CHECK(game.jokers() == 0);
    run(game, 0.3);
    REQUIRE(game.blobs().size() == 1);
    CHECK(game.blobs().front().colors.size() == 1);  // vom grünen Klumpen bleibt eins
    CHECK_FALSE(game.fire_joker());
}

TEST_CASE("Gold: jeder Schuss trifft, gibt Punkte und einen Joker; entwischt kostet es nichts") {
    Game game(manual());
    Blob gold;
    gold.colors = {Color::yellow};
    gold.head = 10.0;
    gold.kind = BlobKind::gold;
    game.spawn_blob(gold);
    REQUIRE(game.fire(Color::blue));
    run(game, 0.4);
    CHECK(game.blobs().empty());
    CHECK(game.jokers() == 1);
    CHECK(game.score() == 100);
    CHECK(game.stats().golds == 1);
    gold.head = 1.3;
    game.spawn_blob(gold);
    run(game, 1.0);
    CHECK(game.blobs().empty());
    CHECK(game.lives() == 3);
}

TEST_CASE("Statistik: Schüsse, Treffer und Fehlschüsse werden gezählt") {
    Game game(manual());
    game.spawn_blob({Color::red, Color::green}, 12.0);
    REQUIRE(game.fire(Color::red));
    run(game, 0.4);
    REQUIRE(game.fire(Color::blue));
    run(game, 0.4);
    CHECK(game.stats().shots == 2);
    CHECK(game.stats().hits == 1);
    CHECK(game.stats().misses == 1);
    CHECK(game.stats().best_combo == 1);
}

TEST_CASE("Kanone: nach einem Schuss sperrt sie kurz") {
    Game game(manual());
    CHECK(game.fire(Color::red));
    CHECK_FALSE(game.fire(Color::red));
    game.update(kShotCooldownS + 0.01);
    CHECK(game.fire(Color::red));
}

TEST_CASE("Schuss ins Leere verschwindet am Ende des Streifens") {
    Game game(manual());
    REQUIRE(game.fire(Color::green));
    run(game, 1.0);
    CHECK(game.shots().empty());
    CHECK(game.score() == 0);
}

TEST_CASE("Darstellung: Klumpen und Kanone stehen an der richtigen Zelle") {
    Game game(manual());
    game.spawn_blob({Color::green, Color::blue}, 5.0);
    const auto frame = game.render();
    REQUIRE(frame.size() == 30);
    CHECK(frame[0] == kCannonIdle);
    CHECK(frame[5] == to_rgb(Color::green));
    CHECK(frame[6] == to_rgb(Color::blue));
    CHECK(frame[7] == kBlack);
}

TEST_CASE("Darstellung: nach dem Schuss zeigt die Kanone die geschossene Farbe") {
    Game game(manual());
    REQUIRE(game.fire(Color::blue));
    CHECK(game.render()[0] == to_rgb(Color::blue));
    game.update(kShotCooldownS + 0.01);
    CHECK(game.render()[0] == kCannonIdle);
}

TEST_CASE("Nachschub: erster Klumpen erst nach der Startpause, am fernen Ende") {
    GameConfig c;
    c.cells = 30;
    c.seed = 7;
    Game game(c);
    run(game, kStartDelayS - 0.1);
    CHECK(game.blobs().empty());
    run(game, 0.2);
    REQUIRE(game.blobs().size() == 1);
    const Blob& b = game.blobs().front();
    CHECK(b.head + static_cast<double>(b.colors.size()) <= 30.0);
    CHECK(b.head > 25.0);
}

TEST_CASE("Ablauf: ohne Gegenwehr kostet der erste Klumpen nach Startpause plus Streckenzeit ein Leben") {
    GameConfig c;
    c.cells = 30;
    c.seed = 1;
    Game game(c);
    // Erster Klumpen ab 2 s auf Zelle 29 (bei Größe 1), 30/14 Zellen je Sekunde,
    // Kanone bei head < 1 → nach 2 + 28·14/30 ≈ 15,1 s.
    run(game, 14.8);
    CHECK(game.lives() == 3);
    run(game, 0.6);
    CHECK(game.lives() == 2);
}

TEST_CASE("Zufall: gleiches Seed und gleiche Schritte ergeben dasselbe Spiel") {
    GameConfig c;
    c.cells = 30;
    c.seed = 4242;
    Game a(c);
    Game b(c);
    run(a, 12.0);
    run(b, 12.0);
    REQUIRE_FALSE(a.blobs().empty());
    CHECK(a.blobs() == b.blobs());
}

TEST_CASE("Konfiguration: ungültige Länge, Leben, Joker und Startstufe werden abgelehnt") {
    GameConfig short_strip;
    short_strip.cells = kMinCells - 1;
    CHECK_THROWS_AS(Game{short_strip}, Error);
    GameConfig no_lives;
    no_lives.lives = 0;
    CHECK_THROWS_AS(Game{no_lives}, Error);
    GameConfig many_jokers;
    many_jokers.jokers = kMaxJokers + 1;
    CHECK_THROWS_AS(Game{many_jokers}, Error);
    GameConfig level_zero;
    level_zero.start_level = 0;
    CHECK_THROWS_AS(Game{level_zero}, Error);
    GameConfig level_high;
    level_high.start_level = kMaxStartLevel + 1;
    CHECK_THROWS_AS(Game{level_high}, Error);
}

TEST_CASE("Startstufe: --stufe 4 beginnt mit vier Farben, --stufe 3 mit Warnung vor dem Endgegner") {
    Game four(manual(30, 4));
    CHECK(four.level() == 4);
    CHECK(four.colors_in_play() == 4);
    CHECK_FALSE(four.boss().has_value());
    Game three(manual(30, 3));
    REQUIRE(three.boss().has_value());
    CHECK(three.boss()->warning);
    CHECK(has_event(three.take_events(), EventKind::boss_warning));
}

TEST_CASE("Effektbereich: Leben rot, Lücke, Joker gold, Lücke, Kombo-Leiste zum nächsten Joker") {
    GameConfig c = manual(40);
    c.jokers = 1;
    Game game(c);
    game.spawn_blob(std::vector<Color>(5, Color::red), 25.0);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(game.fire(Color::red));
        run(game, 0.5);
    }
    REQUIRE(game.combo() == 4);
    const auto zone = game.render_zone(12);
    REQUIRE(zone.size() == 12);
    CHECK(zone[0] == Rgb{230, 0, 30});
    CHECK(zone[2] == Rgb{230, 0, 30});
    CHECK(zone[3] == kBlack);
    CHECK(zone[4] == kGold);
    CHECK(zone[5] == kBlack);
    // Kombo-Leiste: 6 Abschnitte, 4 von 8 Treffern bis zum Joker → 3 hell.
    CHECK(zone[6] == Rgb{0, 200, 255});
    CHECK(zone[8] == Rgb{0, 200, 255});
    CHECK(zone[9] != Rgb{0, 200, 255});
    CHECK(game.render_zone(0).empty());
}

TEST_CASE("Effektbereich: im Endgegnerkampf dessen Lebensleiste, bei der Warnung rotes Blinken") {
    Game warn(manual(30, 3));
    bool red_seen = false;
    for (int i = 0; i < 30 && !red_seen; ++i) {
        warn.update(1.0 / 60.0);
        red_seen = warn.render_zone(6)[0] == Rgb{255, 0, 0};
    }
    CHECK(red_seen);
    Game fight(manual(30, 3));
    REQUIRE(fight.spawn_boss(20.0));
    auto full = fight.render_zone(10);
    CHECK(full[9].r > 100);  // volle Leiste: auch der letzte Abschnitt leuchtet
    const Blob* boss = test::find_kind(fight, BlobKind::boss);
    REQUIRE(boss != nullptr);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(fight.fire(fight.effective_color(*test::find_kind(fight, BlobKind::boss), 0)));
        run(fight, 0.5);
    }
    // Der 5. Treffer macht den Wurm wütend – sein oranger Blitz (0,6 s) liegt kurz über dem
    // ganzen Effektbereich. Ohne Abklingen sah der Test die Leiste nicht (gemessen).
    run(fight, 0.7);
    const auto half = fight.render_zone(10);
    CHECK(half[4].r > 100);  // 5 von 10 Gliedern übrig → 5 Abschnitte hell
    CHECK(half[5].r < 40);
}

TEST_CASE("Darstellung: Joker bereit lässt die Kanone im Regenbogen glimmen") {
    GameConfig c = manual();
    c.jokers = 1;
    Game game(c);
    CHECK(game.render()[0] != kCannonIdle);
    REQUIRE(game.fire_joker());
    CHECK(game.render()[0] == kWhite);
}

}  // namespace ldu
