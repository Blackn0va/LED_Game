// Endgegner: Plan je Stufe, Farbwurm, Chamäleon, Kanonier, Biss, kein Nachschub im Kampf.

#include "game/boss.hpp"
#include "game/game.hpp"
#include "game/level.hpp"
#include "game_test_util.hpp"

#include <doctest.h>

#include <vector>

namespace ldu {

using test::find_kind;
using test::has_event;
using test::manual;
using test::run;

TEST_CASE("Endgegner-Plan: jede dritte Stufe, Reihenfolge Wurm – Chamäleon – Kanonier, danach stärker") {
    CHECK_FALSE(is_boss_level(1));
    CHECK_FALSE(is_boss_level(2));
    CHECK(is_boss_level(3));
    CHECK_FALSE(is_boss_level(4));
    CHECK(is_boss_level(6));
    CHECK(boss_plan(3, 30).type == BossType::worm);
    CHECK(boss_plan(6, 30).type == BossType::chameleon);
    CHECK(boss_plan(9, 30).type == BossType::gunner);
    CHECK(boss_plan(12, 30).type == BossType::worm);
    CHECK(boss_plan(12, 30).segments > boss_plan(3, 30).segments);
    CHECK(boss_plan(3, 8).segments == 4);  // nie mehr als der halbe Streifen
    CHECK(level_params(3).boss);
    CHECK_FALSE(level_params(4).boss);
}

TEST_CASE("Farbwurm: ab der Hälfte wütend, besiegt gibt es Leben, Joker, Punkte und die nächste Stufe") {
    Game game(manual(30, 3));
    REQUIRE(game.spawn_boss(14.0));
    REQUIRE(game.boss().has_value());
    CHECK(game.boss()->segments == 10);
    CHECK_FALSE(game.boss()->warning);
    std::vector<GameEvent> events;
    for (int i = 0; i < 10; ++i) {
        const Blob* boss = find_kind(game, BlobKind::boss);
        REQUIRE(boss != nullptr);
        REQUIRE(game.fire(game.effective_color(*boss, 0)));
        run(game, 0.5);
        for (const GameEvent& e : game.take_events()) {
            events.push_back(e);
        }
        if (i == 4) {
            CHECK(game.boss()->enraged);
        }
    }
    CHECK(find_kind(game, BlobKind::boss) == nullptr);
    CHECK(has_event(events, EventKind::boss_enraged));
    CHECK(has_event(events, EventKind::boss_defeated));
    CHECK(game.level() == 4);
    CHECK(game.lives() == 4);
    CHECK(game.jokers() == 1 + 1);  // einer aus der 8er-Kombo, einer für den Sieg
    CHECK(game.stats().bosses == 1);
    CHECK(game.score() >= 500 * 3);
    CHECK_FALSE(game.boss().has_value());
}

TEST_CASE("Biss: Endgegner an der Kanone kostet ein Leben, verliert zwei Glieder und weicht zurück") {
    Game game(manual(30, 3));
    REQUIRE(game.spawn_boss(1.3));
    run(game, 0.3);
    CHECK(game.lives() == 2);
    const Blob* boss = find_kind(game, BlobKind::boss);
    REQUIRE(boss != nullptr);
    CHECK(boss->colors.size() == 8);
    CHECK(boss->head > 5.0);
    CHECK(has_event(game.take_events(), EventKind::boss_bite));
}

TEST_CASE("Chamäleon: alle Glieder tragen die Taktfarbe, der Takt ist vorhersagbar") {
    Game game(manual(30, 6));
    REQUIRE(game.spawn_boss(12.0));
    const BossPlan plan = boss_plan(6, 30);
    const Blob* boss = find_kind(game, BlobKind::boss);
    REQUIRE(boss != nullptr);
    CHECK(boss->shifting == plan.segments);
    CHECK(game.effective_color(*boss, 0) == Color::red);
    CHECK(game.chameleon_color_in(plan.interval_s + 0.05) == Color::green);
    CHECK(game.chameleon_color_in(2.0 * plan.interval_s + 0.05) == Color::blue);
    run(game, plan.interval_s + 0.05);
    boss = find_kind(game, BlobKind::boss);
    REQUIRE(boss != nullptr);
    CHECK(game.effective_color(*boss, 0) == Color::green);
}

TEST_CASE("Chamäleon: Treffer in Taktfarbe kostet ein Glied, Fehlschuss steckt fest und behält seine Farbe") {
    Game game(manual(30, 6));
    REQUIRE(game.spawn_boss(12.0));
    const int segments = boss_plan(6, 30).segments;
    REQUIRE(game.fire(Color::red));
    run(game, 0.3);
    REQUIRE(game.boss().has_value());
    CHECK(game.boss()->segments == segments - 1);
    REQUIRE(game.fire(Color::blue));  // Takt steht noch auf Rot
    run(game, 0.3);
    const Blob* boss = find_kind(game, BlobKind::boss);
    REQUIRE(boss != nullptr);
    CHECK(boss->colors.size() == static_cast<std::size_t>(segments));
    CHECK(boss->shifting == segments - 1);
    CHECK(game.effective_color(*boss, 0) == Color::blue);
    REQUIRE(game.fire(Color::blue));
    run(game, 0.3);
    CHECK(game.boss()->segments == segments - 1);
    CHECK(find_kind(game, BlobKind::boss)->shifting == static_cast<int>(find_kind(game, BlobKind::boss)->colors.size()));
    // Nach dem Takt gilt die neue Farbe – gespeichert ist das Glied weiter als Rot. Ohne
    // diesen Schritt bestand der Test auch mit abgeschaltetem Farbtakt (Gegenprobe
    // 2026-10-18), weil Rot zufällig die erste Taktfarbe ist.
    run(game, boss_plan(6, 30).interval_s);
    const Color now = game.effective_color(*find_kind(game, BlobKind::boss), 0);
    REQUIRE(now != Color::red);
    REQUIRE(game.fire(now));
    run(game, 0.3);
    CHECK(game.boss()->segments == segments - 2);
}

TEST_CASE("Kanonier: läuft bis zu seinem Standplatz, bleibt stehen und schießt schnelle Einzelpunkte") {
    Game game(manual(30, 9));
    REQUIRE(game.spawn_boss(25.0));
    const Blob* projectile = nullptr;
    for (int i = 0; i < 60 * 8 && projectile == nullptr; ++i) {
        game.update(1.0 / 60.0);
        projectile = find_kind(game, BlobKind::normal);
    }
    REQUIRE(projectile != nullptr);
    const Blob* boss = find_kind(game, BlobKind::boss);
    REQUIRE(boss != nullptr);
    CHECK(boss->head == doctest::Approx(16.0));
    CHECK(projectile->head < boss->head - 1.0);
    CHECK(projectile->speed > 1.0);
    CHECK(has_event(game.take_events(), EventKind::boss_shot));
    run(game, 0.5);
    CHECK(find_kind(game, BlobKind::boss)->head == doctest::Approx(16.0));
}

// Zeitplan: erstes Geschoss bei 0,63 s (35 % von 1,8 s), Treffer am Kanonier bei ~1,2 s.
// Er weicht dabei eine Zelle zurück, läuft ~0,25 s an seinen Platz und schießt 0,25 s
// später – gerechnet ~0,5 s nach dem Treffer. Ohne Gegenschuss käme das nächste Geschoss
// erst bei 2,43 s; geprüft wird bei 1,93 s.
TEST_CASE("Kanonier: auf einen Treffer folgt binnen einer Sekunde ein Gegenschuss") {
    Game game(manual(30, 9));
    REQUIRE(game.spawn_boss(16.0));  // gleich am Standplatz
    const Blob* projectile = nullptr;
    for (int i = 0; i < 60 * 2 && projectile == nullptr; ++i) {
        game.update(1.0 / 60.0);
        projectile = find_kind(game, BlobKind::normal);
    }
    REQUIRE(projectile != nullptr);
    // Erstes Geschoss abwehren – danach steht der Takt wieder auf vollen 1,8 s.
    REQUIRE(game.fire(projectile->colors.front()));
    run(game, 0.3);
    REQUIRE(find_kind(game, BlobKind::normal) == nullptr);
    const Blob* boss = find_kind(game, BlobKind::boss);
    REQUIRE(boss != nullptr);
    REQUIRE(game.fire(game.effective_color(*boss, 0)));
    run(game, 1.0);
    CHECK(game.boss()->segments == boss_plan(9, 30).segments - 1);
    CHECK(find_kind(game, BlobKind::normal) != nullptr);
}

TEST_CASE("Endgegner-Stufe: Warnung, dann der Endgegner – und kein normaler Nachschub") {
    GameConfig c = manual(30, 3);
    c.auto_spawn = true;
    Game game(c);
    run(game, kBossWarningS - 0.1);
    CHECK(game.blobs().empty());
    run(game, 0.3);
    REQUIRE(game.blobs().size() == 1);
    CHECK(game.blobs().front().kind == BlobKind::boss);
    run(game, 6.0);
    CHECK(game.blobs().size() == 1);
}

TEST_CASE("Joker trifft auch das Chamäleon unabhängig vom Takt") {
    GameConfig c = manual(30, 6);
    c.jokers = 1;
    Game game(c);
    REQUIRE(game.spawn_boss(12.0));
    REQUIRE(game.fire_joker());
    run(game, 0.3);
    CHECK(game.boss()->segments == boss_plan(6, 30).segments - 1);
}

}  // namespace ldu
