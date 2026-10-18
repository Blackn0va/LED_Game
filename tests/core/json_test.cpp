// JSON-Leser: was er annimmt, was er ablehnt, und dass Netzdaten ihn nicht aushebeln.

#include "core/json.hpp"

#include <doctest.h>

#include <ostream>
#include <string>

namespace ldu {

TEST_CASE("JSON: verschachteltes Objekt – Werte unter dem richtigen Schlüssel") {
    const auto v = parse_json(R"({"name":"Flur","leds":{"count":150,"name":"innen"},"ok":true,"x":null,"n":-2.5e1})");
    REQUIRE(v);
    REQUIRE(v->is_object());
    CHECK(v->find("name")->as_string() == "Flur");
    CHECK(v->find("leds")->find("count")->as_int() == 150);
    CHECK(v->find("leds")->find("name")->as_string() == "innen");
    CHECK(v->find("ok")->as_bool() == true);
    CHECK(v->find("x")->type() == JsonValue::Type::null);
    CHECK(v->find("n")->as_number() == -25.0);
    CHECK(v->find("fehlt") == nullptr);
    CHECK(v->find("name")->find("x") == nullptr);  // kein Objekt
}

TEST_CASE("JSON: Escapes und \\u-Folgen werden zu UTF-8, Ersatzpaare inklusive") {
    const auto v = parse_json(R"(["a\"b\\c\/d\n", "Grün", "😀"])");
    REQUIRE(v);
    REQUIRE(v->type() == JsonValue::Type::array);
    const auto top = parse_json(R"({"s":"Grün"})");
    REQUIRE(top);
    CHECK(top->find("s")->as_string() == "Grün");
    const auto emoji = parse_json(R"({"s":"😀"})");
    REQUIRE(emoji);
    CHECK(emoji->find("s")->as_string() == "\xF0\x9F\x98\x80");
}

TEST_CASE("JSON: Formfehler liefern nullopt statt geratener Werte") {
    CHECK_FALSE(parse_json(""));
    CHECK_FALSE(parse_json("{"));
    CHECK_FALSE(parse_json(R"({"a":1,})"));
    CHECK_FALSE(parse_json(R"({"a" 1})"));
    CHECK_FALSE(parse_json("01"));
    CHECK_FALSE(parse_json(".5"));
    CHECK_FALSE(parse_json("1."));
    CHECK_FALSE(parse_json("tru"));
    CHECK_FALSE(parse_json(R"("offen)"));
    CHECK_FALSE(parse_json("\"tab\there\""));       // Steuerzeichen roh in der Zeichenkette
    CHECK_FALSE(parse_json(R"("\ud83d")"));         // hohes Surrogat allein
    CHECK_FALSE(parse_json(R"("\ude00")"));         // tiefes Surrogat allein
    CHECK_FALSE(parse_json(R"("\x41")"));           // kein JSON-Escape
    CHECK_FALSE(parse_json("{} {}"));               // zwei Werte
    CHECK(parse_json(" \n{ } \r\n"));               // Leerraum ringsum ist erlaubt
}

TEST_CASE("JSON: zu tiefe Schachtelung wird abgelehnt, die Grenze selbst geht") {
    std::string ok(static_cast<std::size_t>(kJsonMaxDepth) + 1, '[');
    ok += std::string(static_cast<std::size_t>(kJsonMaxDepth) + 1, ']');
    CHECK(parse_json(ok));
    std::string deep(10000, '[');
    deep += std::string(10000, ']');
    CHECK_FALSE(parse_json(deep));  // ohne Grenze liefe der Stapel über
}

TEST_CASE("JSON: as_int nur für ganze Zahlen im int-Bereich") {
    const auto v = parse_json(R"({"a":3,"b":3.5,"c":1e10,"d":"3"})");
    REQUIRE(v);
    CHECK(v->find("a")->as_int() == 3);
    CHECK_FALSE(v->find("b")->as_int());
    CHECK_FALSE(v->find("c")->as_int());
    CHECK_FALSE(v->find("d")->as_int());
}

}  // namespace ldu
