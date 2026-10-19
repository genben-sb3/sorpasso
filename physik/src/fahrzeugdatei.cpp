#include "ultragravity/fahrzeugdatei.h"

#include <algorithm>
#include <fstream>
#include <locale>
#include <sstream>

#include "nlohmann/json.hpp"

namespace ultragravity {
namespace {

using nlohmann::json;

std::string unter(const std::string& pfad, const std::string& name) {
    return pfad.empty() ? name : pfad + "." + name;
}

// Liest Felder mit Pfadangabe. Nach dem ersten Fehler liefert er nur noch
// Nullwerte, damit der Ladecode ohne Exceptions geradeaus lesen kann.
class Leser {
public:
    std::optional<LadeFehler> fehler;

    const json& objekt(const json& eltern, const std::string& pfad, const std::string& name) {
        const json* feld = finde(eltern, pfad, name);
        if (feld && !feld->is_object()) {
            melde(unter(pfad, name), "ist kein Objekt");
            return leer_;
        }
        return feld ? *feld : leer_;
    }

    double zahl(const json& eltern, const std::string& pfad, const std::string& name) {
        const json* feld = finde(eltern, pfad, name);
        if (feld && !feld->is_number()) {
            melde(unter(pfad, name), "ist keine Zahl");
            return 0.0;
        }
        return feld ? feld->get<double>() : 0.0;
    }

    std::string text(const json& eltern, const std::string& pfad, const std::string& name) {
        const json* feld = finde(eltern, pfad, name);
        if (feld && !feld->is_string()) {
            melde(unter(pfad, name), "ist kein Text");
            return {};
        }
        return feld ? feld->get<std::string>() : std::string{};
    }

    Vec3 vektor(const json& eltern, const std::string& pfad, const std::string& name) {
        const json& o = objekt(eltern, pfad, name);
        const std::string p = unter(pfad, name);
        return {zahl(o, p, "x"), zahl(o, p, "y"), zahl(o, p, "z")};
    }

    void melde(std::string feld, std::string meldung) {
        if (!fehler) fehler = LadeFehler{std::move(feld), std::move(meldung)};
    }

private:
    const json* finde(const json& eltern, const std::string& pfad, const std::string& name) {
        if (fehler) return nullptr;
        const auto it = eltern.find(name);
        if (it == eltern.end()) {
            melde(unter(pfad, name), "fehlt");
            return nullptr;
        }
        return &*it;
    }

    const json leer_ = json::object();
};

Fahrwerk lies_fahrwerk(Leser& leser, const json& o, const std::string& pfad) {
    Fahrwerk f;
    f.federrate = leser.zahl(o, pfad, "federrate");
    f.daempfer_druck = leser.zahl(o, pfad, "daempfer_druck");
    f.daempfer_zug = leser.zahl(o, pfad, "daempfer_zug");
    f.federweg = leser.zahl(o, pfad, "federweg");
    f.anschlagrate = leser.zahl(o, pfad, "anschlagrate");
    return f;
}

Achse lies_achse(Leser& leser, const json& achsen, const std::string& name) {
    const std::string pfad = unter("achsen", name);
    const json& o = leser.objekt(achsen, "achsen", name);
    Achse a;
    a.x = leser.zahl(o, pfad, "x");
    a.spurweite = leser.zahl(o, pfad, "spurweite");
    a.anlenkhoehe = leser.zahl(o, pfad, "anlenkhoehe");
    a.radradius = leser.zahl(o, pfad, "radradius");
    a.fahrwerk = lies_fahrwerk(leser, leser.objekt(o, pfad, "fahrwerk"), unter(pfad, "fahrwerk"));
    return a;
}

std::string zahl_als_text(double wert) {
    std::ostringstream s;
    s.imbue(std::locale::classic()); // Punkt als Dezimaltrenner, egal was der Host eingestellt hat
    s << wert;
    return s.str();
}

// Ein benannter Grenzwert, damit die Meldung sagt, woran der Wert scheitert.
struct Grenze {
    double wert;
    std::string name;
};

// Lehnt Werte ab, die kein fahrbares Auto ergeben. Liefert den ersten Verstoß.
class Plausibilitaet {
public:
    std::optional<LadeFehler> fehler;

    void positiv(double wert, const std::string& feld) {
        if (!(wert > 0.0)) melde(feld, "muss größer als 0 sein");
    }

    void nicht_negativ(double wert, const std::string& feld) {
        if (!(wert >= 0.0)) melde(feld, "darf nicht negativ sein");
    }

    void zwischen(double wert, const Grenze& unten, const Grenze& oben, const std::string& feld) {
        if (!(wert > unten.wert && wert < oben.wert)) {
            melde(feld, "muss zwischen " + unten.name + " (" + zahl_als_text(unten.wert) + ") und " + oben.name +
                            " (" + zahl_als_text(oben.wert) + ") liegen");
        }
    }

    void melde(const std::string& feld, std::string meldung) {
        if (!fehler) fehler = LadeFehler{feld, std::move(meldung)};
    }
};

void pruefe_achse(Plausibilitaet& p, const Achse& a, const std::string& pfad) {
    p.positiv(a.spurweite, unter(pfad, "spurweite"));
    p.positiv(a.anlenkhoehe, unter(pfad, "anlenkhoehe"));
    p.positiv(a.radradius, unter(pfad, "radradius"));
    const std::string fw = unter(pfad, "fahrwerk");
    p.positiv(a.fahrwerk.federrate, unter(fw, "federrate"));
    p.nicht_negativ(a.fahrwerk.daempfer_druck, unter(fw, "daempfer_druck"));
    p.nicht_negativ(a.fahrwerk.daempfer_zug, unter(fw, "daempfer_zug"));
    p.positiv(a.fahrwerk.federweg, unter(fw, "federweg"));
    p.nicht_negativ(a.fahrwerk.anschlagrate, unter(fw, "anschlagrate"));
}

std::optional<LadeFehler> pruefe(const Kennwerte& k) {
    Plausibilitaet p;
    p.positiv(k.masse, "masse");
    p.positiv(k.traegheit.x, "traegheit.x");
    p.positiv(k.traegheit.y, "traegheit.y");
    p.positiv(k.traegheit.z, "traegheit.z");
    pruefe_achse(p, k.vorn, "achsen.vorn");
    pruefe_achse(p, k.hinten, "achsen.hinten");
    if (!(k.vorn.x > k.hinten.x)) {
        p.melde("achsen.vorn.x", "muss vor der Hinterachse (" + zahl_als_text(k.hinten.x) + ") liegen");
    }
    p.zwischen(k.schwerpunkt.x, {k.hinten.x, "Hinterachse"}, {k.vorn.x, "Vorderachse"}, "schwerpunkt.x");
    // ISO 8855: y zeigt nach links. Maßgeblich ist die schmalere Spur.
    const double halbe_spur = std::min(k.vorn.spurweite, k.hinten.spurweite) / 2.0;
    p.zwischen(k.schwerpunkt.y, {-halbe_spur, "rechtem Rad"}, {halbe_spur, "linkem Rad"}, "schwerpunkt.y");
    p.positiv(k.schwerpunkt.z, "schwerpunkt.z");
    return p.fehler;
}

// Prüft nur die JSON-Syntax und merkt sich, wo sie bricht. Der DOM-Parser
// sagt im Modus ohne Exceptions bloß, dass etwas kaputt ist, nicht wo.
struct SyntaxPruefer {
    std::size_t gelesen = 0; // Bytes bis einschließlich der Stelle, an der der Fehler auffiel

    bool null() { return true; }
    bool boolean(bool) { return true; }
    bool number_integer(json::number_integer_t) { return true; }
    bool number_unsigned(json::number_unsigned_t) { return true; }
    bool number_float(json::number_float_t, const json::string_t&) { return true; }
    bool string(json::string_t&) { return true; }
    bool binary(json::binary_t&) { return true; }
    bool start_object(std::size_t) { return true; }
    bool key(json::string_t&) { return true; }
    bool end_object() { return true; }
    bool start_array(std::size_t) { return true; }
    bool end_array() { return true; }
    bool parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception&) {
        gelesen = position;
        return false;
    }
};

// Zeile und Spalte (ab 1) des zuletzt gelesenen Bytes. Spalten zählen
// UTF-8-Zeichen, damit sie zu dem passen, was ein Editor anzeigt.
LadeFehler syntaxfehler(std::string_view text, std::size_t gelesen) {
    const std::size_t stelle = std::min(text.size(), gelesen > 0 ? gelesen - 1 : 0);
    std::size_t zeile = 1;
    std::size_t spalte = 1;
    for (std::size_t i = 0; i < stelle; ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte == '\n') {
            ++zeile;
            spalte = 1;
        } else if ((byte & 0xC0) != 0x80) { // Folgebytes gehören zum Zeichen davor
            ++spalte;
        }
    }
    return {"", "kein gültiges JSON in Zeile " + std::to_string(zeile) + ", Spalte " + std::to_string(spalte)};
}

} // namespace

LadeErgebnis lies_fahrzeugdatei(std::string_view text) {
    SyntaxPruefer pruefer;
    if (!json::sax_parse(text, &pruefer)) return {{}, syntaxfehler(text, pruefer.gelesen)};
    const json wurzel = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (!wurzel.is_object()) return {{}, LadeFehler{"", "ist kein Objekt"}};

    Leser leser;
    Kennwerte k;
    k.name = leser.text(wurzel, "", "name");
    k.masse = leser.zahl(wurzel, "", "masse");
    k.traegheit = leser.vektor(wurzel, "", "traegheit");
    k.schwerpunkt = leser.vektor(wurzel, "", "schwerpunkt");
    const json& achsen = leser.objekt(wurzel, "", "achsen");
    k.vorn = lies_achse(leser, achsen, "vorn");
    k.hinten = lies_achse(leser, achsen, "hinten");

    if (leser.fehler) return {{}, leser.fehler};
    if (auto verstoss = pruefe(k)) return {{}, std::move(verstoss)};
    return {std::move(k), std::nullopt};
}

LadeErgebnis lade_fahrzeugdatei(const std::string& pfad) {
    std::ifstream datei(pfad, std::ios::binary);
    if (!datei) return {{}, LadeFehler{"", "Datei lässt sich nicht öffnen: " + pfad}};
    std::ostringstream inhalt;
    inhalt << datei.rdbuf();
    return lies_fahrzeugdatei(inhalt.str());
}

} // namespace ultragravity
