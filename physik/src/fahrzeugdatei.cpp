#include "ultragravity/fahrzeugdatei.h"

#include <algorithm>
#include <fstream>
#include <locale>
#include <numbers>
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

    // Eine Liste mit Pfadangabe. Ihre Elemente heißen pfad[0], pfad[1], …
    const json& liste(const json& eltern, const std::string& pfad, const std::string& name) {
        const json* feld = finde(eltern, pfad, name);
        if (feld && !feld->is_array()) {
            melde(unter(pfad, name), "ist keine Liste");
            return leere_liste_;
        }
        return feld ? *feld : leere_liste_;
    }

    const json& objekt(const json& element, const std::string& pfad) {
        if (fehler) return leer_;
        if (!element.is_object()) {
            melde(pfad, "ist kein Objekt");
            return leer_;
        }
        return element;
    }

    double zahl(const json& element, const std::string& pfad) {
        if (fehler) return 0.0;
        if (!element.is_number()) {
            melde(pfad, "ist keine Zahl");
            return 0.0;
        }
        return element.get<double>();
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
    const json leere_liste_ = json::array();
};

Fahrwerk lies_fahrwerk(Leser& leser, const json& o, const std::string& pfad) {
    Fahrwerk f;
    f.federrate = leser.zahl(o, pfad, "federrate");
    f.daempfer_druck = leser.zahl(o, pfad, "daempfer_druck");
    f.daempfer_zug = leser.zahl(o, pfad, "daempfer_zug");
    f.federweg = leser.zahl(o, pfad, "federweg");
    f.anschlagrate = leser.zahl(o, pfad, "anschlagrate");
    f.stabilisator = leser.zahl(o, pfad, "stabilisator");
    return f;
}

Kurvenform lies_kurvenform(Leser& leser, const json& o, const std::string& pfad) {
    Kurvenform k;
    k.steifigkeit = leser.zahl(o, pfad, "steifigkeit");
    k.form = leser.zahl(o, pfad, "form");
    k.kruemmung = leser.zahl(o, pfad, "kruemmung");
    return k;
}

Reifen lies_reifen(Leser& leser, const json& o, const std::string& pfad) {
    Reifen r;
    r.spitzenreibwert = leser.zahl(o, pfad, "spitzenreibwert");
    r.nennlast = leser.zahl(o, pfad, "nennlast");
    r.lastempfindlichkeit = leser.zahl(o, pfad, "lastempfindlichkeit");
    r.traegheit = leser.zahl(o, pfad, "traegheit");
    r.relaxationslaenge = leser.zahl(o, pfad, "relaxationslaenge");
    r.laengs = lies_kurvenform(leser, leser.objekt(o, pfad, "laengs"), unter(pfad, "laengs"));
    r.quer = lies_kurvenform(leser, leser.objekt(o, pfad, "quer"), unter(pfad, "quer"));
    const std::string kombiniert = unter(pfad, "kombiniert");
    const json& g = leser.objekt(o, pfad, "kombiniert");
    r.kombiniert.laengs = leser.zahl(g, kombiniert, "laengs");
    r.kombiniert.quer = leser.zahl(g, kombiniert, "quer");
    return r;
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
    a.reifen = lies_reifen(leser, leser.objekt(o, pfad, "reifen"), unter(pfad, "reifen"));
    return a;
}

std::string element(const std::string& pfad, std::size_t index) { return pfad + "[" + std::to_string(index) + "]"; }

constexpr double u_min = 2.0 * std::numbers::pi / 60.0; // rad/s je U/min
constexpr double grad = std::numbers::pi / 180.0;       // rad je Grad

Antrieb lies_antrieb(Leser& leser, const json& wurzel) {
    Antrieb a;
    const json& o = leser.objekt(wurzel, "", "antrieb");

    const json& m = leser.objekt(o, "antrieb", "motor");
    const std::string motor = "antrieb.motor";
    const json& kurve = leser.liste(m, motor, "drehmoment");
    for (std::size_t i = 0; i < kurve.size(); ++i) {
        const std::string pfad = element(unter(motor, "drehmoment"), i);
        const json& punkt = leser.objekt(kurve[i], pfad);
        a.motor.drehmoment.push_back({leser.zahl(punkt, pfad, "u_min") * u_min, leser.zahl(punkt, pfad, "moment")});
    }
    a.motor.leerlaufdrehzahl = leser.zahl(m, motor, "leerlauf_u_min") * u_min;
    a.motor.hoechstdrehzahl = leser.zahl(m, motor, "hoechst_u_min") * u_min;
    a.motor.traegheit = leser.zahl(m, motor, "traegheit");
    a.motor.schleppmoment = leser.zahl(m, motor, "schleppmoment");

    a.kupplung.moment = leser.zahl(leser.objekt(o, "antrieb", "kupplung"), "antrieb.kupplung", "moment");

    const json& g = leser.objekt(o, "antrieb", "getriebe");
    const std::string getriebe = "antrieb.getriebe";
    const json& gaenge = leser.liste(g, getriebe, "uebersetzungen");
    for (std::size_t i = 0; i < gaenge.size(); ++i) {
        a.getriebe.uebersetzungen.push_back(leser.zahl(gaenge[i], element(unter(getriebe, "uebersetzungen"), i)));
    }
    a.getriebe.achsuebersetzung = leser.zahl(g, getriebe, "achsuebersetzung");
    a.getriebe.schaltzeit = leser.zahl(g, getriebe, "schaltzeit");

    // Optionale Blöcke liest er nur, wenn die Wahl sie verlangt; liegt einer ohne
    // diese Wahl in der Datei, fällt das in der Plausibilitätsprüfung auf.
    const std::string modus = leser.text(g, getriebe, "modus");
    if (modus == "manuell") {
        a.getriebe.modus = Getriebemodus::manuell;
    } else if (modus == "automatik") {
        a.getriebe.modus = Getriebemodus::automatik;
        const std::string pfad = unter(getriebe, "automatik");
        const json& au = leser.objekt(g, getriebe, "automatik");
        Automatik& m = a.getriebe.automatik;
        m.hochschaltdrehzahl = leser.zahl(au, pfad, "hochschalt_u_min") * u_min;
        m.runterschaltdrehzahl = leser.zahl(au, pfad, "runterschalt_u_min") * u_min;
        m.bremsschaltdrehzahl = leser.zahl(au, pfad, "bremsschalt_u_min") * u_min;
        m.haltezeit = leser.zahl(au, pfad, "haltezeit");
        m.einkuppelbereich = leser.zahl(au, pfad, "einkuppelbereich_u_min") * u_min;
        m.einkuppelzeit = leser.zahl(au, pfad, "einkuppelzeit");
        m.driftwinkel = leser.zahl(au, pfad, "driftwinkel_grad") * grad;
    } else if (!leser.fehler) {
        leser.melde(unter(getriebe, "modus"), "muss \"manuell\" oder \"automatik\" sein");
    }

    const std::string art = leser.text(o, "antrieb", "art");
    if (art == "heck") {
        a.art = Antriebsart::heck;
    } else if (art == "front") {
        a.art = Antriebsart::front;
    } else if (art == "allrad") {
        a.art = Antriebsart::allrad;
        a.momentverteilung_vorn = leser.zahl(o, "antrieb", "momentverteilung_vorn");
    } else if (!leser.fehler) {
        leser.melde("antrieb.art", "muss \"heck\", \"front\" oder \"allrad\" sein");
    }

    const std::string differenzial = "antrieb.differenzial";
    const json& d = leser.objekt(o, "antrieb", "differenzial");
    const std::string sperre = leser.text(d, differenzial, "art");
    if (sperre == "offen") {
        a.differenzial.sperrt = false;
    } else if (sperre == "sperre") {
        a.differenzial.sperrt = true;
        a.differenzial.vorspannung = leser.zahl(d, differenzial, "vorspannung");
        a.differenzial.sperrwirkung = leser.zahl(d, differenzial, "sperrwirkung");
    } else if (!leser.fehler) {
        leser.melde(unter(differenzial, "art"), "muss \"offen\" oder \"sperre\" sein");
    }
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

    void hoechstens(double wert, double grenze, const std::string& feld) {
        if (!(wert <= grenze)) melde(feld, "darf höchstens " + zahl_als_text(grenze) + " sein");
    }

    // Wie zwischen, aber mit eingeschlossenen Grenzen ohne eigene Namen.
    void bereich(double wert, double unten, double oben, const std::string& feld) {
        if (!(wert >= unten && wert <= oben)) {
            melde(feld, "muss zwischen " + zahl_als_text(unten) + " und " + zahl_als_text(oben) + " liegen");
        }
    }

    void zwischen(double wert, const Grenze& unten, const Grenze& oben, const std::string& feld) {
        if (!(wert > unten.wert && wert < oben.wert)) {
            melde(feld, "muss zwischen " + unten.name + " (" + zahl_als_text(unten.wert) + ") und " + oben.name +
                            " (" + zahl_als_text(oben.wert) + ") liegen");
        }
    }

    void nicht_leer(std::size_t anzahl, const std::string& feld) {
        if (anzahl == 0) melde(feld, "darf nicht leer sein");
    }

    void melde(const std::string& feld, std::string meldung) {
        if (!fehler) fehler = LadeFehler{feld, std::move(meldung)};
    }
};

// Mit C über 2 oder E über 1 verliert die Kurve ihre Form: Sie kippt hinter
// dem Spitzenwert ins Negative oder hat mehrere Spitzen.
void pruefe_kurvenform(Plausibilitaet& p, const Kurvenform& k, const std::string& pfad) {
    p.positiv(k.steifigkeit, unter(pfad, "steifigkeit"));
    p.positiv(k.form, unter(pfad, "form"));
    p.hoechstens(k.form, 2.0, unter(pfad, "form"));
    p.hoechstens(k.kruemmung, 1.0, unter(pfad, "kruemmung"));
}

void pruefe_reifen(Plausibilitaet& p, const Reifen& r, const std::string& pfad) {
    p.positiv(r.spitzenreibwert, unter(pfad, "spitzenreibwert"));
    p.positiv(r.nennlast, unter(pfad, "nennlast"));
    p.bereich(r.lastempfindlichkeit, 0.0, 1.0, unter(pfad, "lastempfindlichkeit"));
    p.positiv(r.traegheit, unter(pfad, "traegheit"));
    p.positiv(r.relaxationslaenge, unter(pfad, "relaxationslaenge"));
    pruefe_kurvenform(p, r.laengs, unter(pfad, "laengs"));
    pruefe_kurvenform(p, r.quer, unter(pfad, "quer"));
    p.nicht_negativ(r.kombiniert.laengs, unter(pfad, "kombiniert.laengs"));
    p.nicht_negativ(r.kombiniert.quer, unter(pfad, "kombiniert.quer"));
}

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
    p.nicht_negativ(a.fahrwerk.stabilisator, unter(fw, "stabilisator"));
    pruefe_reifen(p, a.reifen, unter(pfad, "reifen"));
}

void pruefe_motor(Plausibilitaet& p, const Motor& m, const std::string& pfad) {
    const std::string kurve = unter(pfad, "drehmoment");
    p.nicht_leer(m.drehmoment.size(), kurve);
    for (std::size_t i = 0; i < m.drehmoment.size(); ++i) {
        const std::string punkt = element(kurve, i);
        if (i == 0) {
            p.positiv(m.drehmoment[i].drehzahl, unter(punkt, "u_min"));
        } else if (!(m.drehmoment[i].drehzahl > m.drehmoment[i - 1].drehzahl)) {
            p.melde(unter(punkt, "u_min"), "muss größer als beim vorigen Stützpunkt sein");
        }
        p.nicht_negativ(m.drehmoment[i].moment, unter(punkt, "moment"));
    }
    p.positiv(m.leerlaufdrehzahl, unter(pfad, "leerlauf_u_min"));
    if (!(m.hoechstdrehzahl > m.leerlaufdrehzahl)) {
        p.melde(unter(pfad, "hoechst_u_min"), "muss über der Leerlaufdrehzahl liegen");
    }
    if (!m.drehmoment.empty() && !(m.hoechstdrehzahl <= m.drehmoment.back().drehzahl)) {
        p.melde(unter(pfad, "hoechst_u_min"), "darf nicht über dem letzten Stützpunkt der Drehmomentkurve liegen");
    }
    p.positiv(m.traegheit, unter(pfad, "traegheit"));
    p.nicht_negativ(m.schleppmoment, unter(pfad, "schleppmoment"));
}

void pruefe_getriebe(Plausibilitaet& p, const Getriebe& g, const std::string& pfad) {
    const std::string gaenge = unter(pfad, "uebersetzungen");
    p.nicht_leer(g.uebersetzungen.size(), gaenge);
    for (std::size_t i = 0; i < g.uebersetzungen.size(); ++i) {
        p.positiv(g.uebersetzungen[i], element(gaenge, i));
        if (i > 0 && !(g.uebersetzungen[i] < g.uebersetzungen[i - 1])) {
            p.melde(element(gaenge, i), "muss kleiner als im vorigen Gang sein");
        }
    }
    p.positiv(g.achsuebersetzung, unter(pfad, "achsuebersetzung"));
    p.nicht_negativ(g.schaltzeit, unter(pfad, "schaltzeit"));
}

// Aufeinanderfolgende Gänge: kleinstes Verhältnis der Übersetzung eines Gangs
// zu der seines Vorgängers, also der größte Drehzahlsprung beim Hochschalten.
double groesster_sprung(const Getriebe& g) {
    double sprung = 1.0;
    for (std::size_t i = 1; i < g.uebersetzungen.size(); ++i) {
        sprung = std::min(sprung, g.uebersetzungen[i] / g.uebersetzungen[i - 1]);
    }
    return sprung;
}

void pruefe_automatik(Plausibilitaet& p, const Antrieb& a, const std::string& pfad) {
    const Automatik& m = a.getriebe.automatik;
    const Motor& motor = a.motor;
    p.zwischen(m.hochschaltdrehzahl / u_min, {motor.leerlaufdrehzahl / u_min, "Leerlaufdrehzahl"},
               {motor.hoechstdrehzahl / u_min, "Höchstdrehzahl"}, unter(pfad, "hochschalt_u_min"));
    p.positiv(m.runterschaltdrehzahl, unter(pfad, "runterschalt_u_min"));
    // Nach dem Hochschalten muss die Drehzahl über der Runterschaltschwelle bleiben,
    // sonst schaltet die Automatik sofort wieder zurück und pendelt.
    if (!(m.runterschaltdrehzahl < m.hochschaltdrehzahl * groesster_sprung(a.getriebe))) {
        p.melde(unter(pfad, "runterschalt_u_min"),
                "muss unter der Drehzahl liegen, auf die das Hochschalten fällt (" +
                    zahl_als_text(m.hochschaltdrehzahl * groesster_sprung(a.getriebe) / u_min) +
                    "), sonst pendelt die Automatik");
    }
    if (!(m.bremsschaltdrehzahl >= m.runterschaltdrehzahl && m.bremsschaltdrehzahl < m.hochschaltdrehzahl)) {
        p.melde(unter(pfad, "bremsschalt_u_min"),
                "muss zwischen runterschalt_u_min und hochschalt_u_min liegen");
    }
    p.positiv(m.haltezeit, unter(pfad, "haltezeit"));
    p.positiv(m.einkuppelbereich, unter(pfad, "einkuppelbereich_u_min"));
    p.nicht_negativ(m.einkuppelzeit, unter(pfad, "einkuppelzeit"));
    p.positiv(m.driftwinkel, unter(pfad, "driftwinkel_grad"));
    p.hoechstens(m.driftwinkel / grad, 90.0, unter(pfad, "driftwinkel_grad"));
}

void pruefe_antriebsart(Plausibilitaet& p, const Antrieb& a) {
    if (a.art == Antriebsart::allrad) {
        p.zwischen(a.momentverteilung_vorn, {0.0, "keinem Moment vorn"}, {1.0, "keinem Moment hinten"},
                   "antrieb.momentverteilung_vorn");
    }
    if (a.differenzial.sperrt) {
        p.nicht_negativ(a.differenzial.vorspannung, "antrieb.differenzial.vorspannung");
        p.nicht_negativ(a.differenzial.sperrwirkung, "antrieb.differenzial.sperrwirkung");
        if (!(a.differenzial.vorspannung > 0.0 || a.differenzial.sperrwirkung > 0.0)) {
            p.melde("antrieb.differenzial.art",
                    "ein Sperrdifferenzial braucht Vorspannung oder Sperrwirkung, sonst ist es offen");
        }
    }
    if (a.getriebe.modus == Getriebemodus::automatik) pruefe_automatik(p, a, "antrieb.getriebe.automatik");
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
    // Bis 60° bleibt der Einschlag weit weg vom Querstellen des Rads.
    p.positiv(k.lenkung.max_lenkwinkel, "lenkung.max_lenkwinkel_grad");
    p.hoechstens(k.lenkung.max_lenkwinkel / grad, 60.0, "lenkung.max_lenkwinkel_grad");
    p.nicht_negativ(k.bremsen.moment, "bremsen.moment");
    p.bereich(k.bremsen.balance, 0.0, 1.0, "bremsen.balance");
    pruefe_motor(p, k.antrieb.motor, "antrieb.motor");
    p.positiv(k.antrieb.kupplung.moment, "antrieb.kupplung.moment");
    pruefe_getriebe(p, k.antrieb.getriebe, "antrieb.getriebe");
    pruefe_antriebsart(p, k.antrieb);
    return p.fehler;
}

// Blöcke, die zur gewählten Antriebsart, zum Differenzial oder zum Getriebemodus
// nicht passen, lehnt er ab, statt sie still zu übergehen.
std::optional<LadeFehler> fremde_bloecke(const json& antrieb) {
    const auto text = [](const json& o, const char* name) { return o.at(name).get<std::string>(); };
    if (antrieb.contains("momentverteilung_vorn") && text(antrieb, "art") != "allrad") {
        return LadeFehler{"antrieb.momentverteilung_vorn", "gibt es nur beim Allradantrieb"};
    }
    const json& differenzial = antrieb.at("differenzial");
    for (const char* name : {"vorspannung", "sperrwirkung"}) {
        if (differenzial.contains(name) && text(differenzial, "art") != "sperre") {
            return LadeFehler{unter("antrieb.differenzial", name), "gibt es nur beim Sperrdifferenzial"};
        }
    }
    const json& getriebe = antrieb.at("getriebe");
    if (getriebe.contains("automatik") && text(getriebe, "modus") != "automatik") {
        return LadeFehler{"antrieb.getriebe.automatik", "gibt es nur im Getriebemodus automatik"};
    }
    return std::nullopt;
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
    k.lenkung.max_lenkwinkel =
        leser.zahl(leser.objekt(wurzel, "", "lenkung"), "lenkung", "max_lenkwinkel_grad") * grad;
    const json& bremsen = leser.objekt(wurzel, "", "bremsen");
    k.bremsen.moment = leser.zahl(bremsen, "bremsen", "moment");
    k.bremsen.balance = leser.zahl(bremsen, "bremsen", "balance");
    k.antrieb = lies_antrieb(leser, wurzel);

    if (leser.fehler) return {{}, leser.fehler};
    if (auto verstoss = fremde_bloecke(wurzel.at("antrieb"))) return {{}, std::move(verstoss)};
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
