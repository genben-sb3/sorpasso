#include "ultragravity/fahrzeugdatei.h"

#include <fstream>
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

} // namespace

LadeErgebnis lies_fahrzeugdatei(std::string_view text) {
    const json wurzel = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (wurzel.is_discarded()) return {{}, LadeFehler{"", "kein gültiges JSON"}};

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
