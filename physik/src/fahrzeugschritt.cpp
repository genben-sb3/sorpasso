#include "ultragravity/fahrzeugschritt.h"

#include <algorithm>
#include <cmath>

namespace ultragravity {
namespace {

struct Radaufhaengung {
    const Achse* achse;
    Vec3 anlenkpunkt; // im Fahrzeugsystem, relativ zum Schwerpunkt
};

std::array<Radaufhaengung, anzahl_raeder> radaufhaengungen(const Kennwerte& k) {
    const auto anlenkpunkt = [&](const Achse& a, double seite) {
        return Vec3{a.x, seite * a.spurweite / 2.0, a.anlenkhoehe} - k.schwerpunkt;
    };
    return {{
        {&k.vorn, anlenkpunkt(k.vorn, 1.0)},
        {&k.vorn, anlenkpunkt(k.vorn, -1.0)},
        {&k.hinten, anlenkpunkt(k.hinten, 1.0)},
        {&k.hinten, anlenkpunkt(k.hinten, -1.0)},
    }};
}

double federkraft(const Fahrwerk& f, double einfederung, double einfedergeschwindigkeit) {
    double kraft = f.federrate * einfederung;
    if (einfederung > f.federweg) kraft += f.anschlagrate * (einfederung - f.federweg);
    const double daempfung = einfedergeschwindigkeit > 0.0 ? f.daempfer_druck : f.daempfer_zug;
    kraft += daempfung * einfedergeschwindigkeit;
    // Der Boden kann nur drücken, nicht ziehen.
    return std::max(kraft, 0.0);
}

// Unterhalb dieser Geschwindigkeit geht der Schlupfnenner glatt in einen festen
// Wert über, damit der Schlupf bei Stillstand nicht gegen unendlich läuft.
// Zwischenschritte für Räder und Reifen je Fahrzeug-Schritt (ADR-0006).
constexpr int zwischenschritte = 8;

constexpr double schlupfnenner_minimum = 1.0; // m/s

double schlupfnenner(double v) { return std::sqrt(v * v + schlupfnenner_minimum * schlupfnenner_minimum); }

// Magic Formula über einem Schlupf, normiert auf den Spitzenwert 1, samt ihrer
// Ableitung nach dem Schlupf.
struct Kurvenpunkt {
    double wert;
    double steigung;
};

Kurvenpunkt magic_formula(const Kurvenform& k, double schlupf) {
    const double b = k.steifigkeit;
    const double bx = b * schlupf;
    const double phi = bx - k.kruemmung * (bx - std::atan(bx));
    const double dphi = b - k.kruemmung * (b - b / (1.0 + bx * bx));
    const double winkel = k.form * std::atan(phi);
    return {std::sin(winkel), std::cos(winkel) * k.form / (1.0 + phi * phi) * dphi};
}

// Reibwert zwischen Reifen und Belag. Mehr Radlast bringt unterproportional
// mehr Kraft (Lastempfindlichkeit).
double reibwert(const Reifen& r, double radlast, double belag) {
    const double mehrlast = (radlast - r.nennlast) / r.nennlast;
    return std::max(belag * r.spitzenreibwert * (1.0 - r.lastempfindlichkeit * mehrlast), 0.0);
}

// Unterhalb der Leerlaufdrehzahl gibt der Leerlaufregler über diesen Bereich
// hinweg von null bis Vollgas.
constexpr double leerlauf_regelbereich = 100.0 * 2.0 * 3.14159265358979323846 / 60.0; // rad/s

// Drehmoment bei Vollgas, linear zwischen den Stützpunkten, außerhalb der Kurve
// ihr Randwert.
double vollgasmoment(const Motor& m, double drehzahl) {
    const auto& kurve = m.drehmoment;
    if (drehzahl <= kurve.front().drehzahl) return kurve.front().moment;
    for (std::size_t i = 1; i < kurve.size(); ++i) {
        if (drehzahl <= kurve[i].drehzahl) {
            const Stuetzpunkt& a = kurve[i - 1];
            const Stuetzpunkt& b = kurve[i];
            return a.moment + (b.moment - a.moment) * (drehzahl - a.drehzahl) / (b.drehzahl - a.drehzahl);
        }
    }
    return kurve.back().moment;
}

// Moment des Motors an der Kurbelwelle: Der Leerlaufregler gibt unter der
// Leerlaufdrehzahl Gas, der Begrenzer nimmt es über der Höchstdrehzahl weg, und
// ohne Gas schleppt der Motor (Motorbremse).
double motormoment(const Motor& m, double drehzahl, double gas) {
    const double regler = std::clamp((m.leerlaufdrehzahl - drehzahl) / leerlauf_regelbereich, 0.0, 1.0);
    const double wirksam = drehzahl >= m.hoechstdrehzahl ? 0.0 : std::max(gas, regler);
    const double schlepp = m.schleppmoment * std::max(drehzahl, 0.0) / m.hoechstdrehzahl;
    return wirksam * vollgasmoment(m, drehzahl) - (1.0 - wirksam) * schlepp;
}

// Was ein Rad über alle Zwischenschritte eines Schritts gleich bleibt.
struct Radlage {
    const Achse* achse = nullptr;
    bool kontakt = false;
    Vec3 hebel;           // vom Schwerpunkt zum Kontaktpunkt, Weltsystem
    Vec3 laengs;          // Rollrichtung in der Bodenebene
    Vec3 v_kontakt;              // Geschwindigkeit der Karosserie im Kontaktpunkt
    double haftkraft = 0.0;      // N, Radlast mal Reibwert
    double bremsmoment = 0.0;    // N·m
};

} // namespace

Zustand rollender_zustand(const Kennwerte& kennwerte, double geschwindigkeit) {
    Zustand z;
    const auto aufhaengungen = radaufhaengungen(kennwerte);
    for (int i = 0; i < anzahl_raeder; ++i) z.raddrehzahl[i] = geschwindigkeit / aufhaengungen[i].achse->radradius;
    z.motordrehzahl = kennwerte.antrieb.motor.leerlaufdrehzahl;
    return z;
}

std::array<Abtaststrahl, anzahl_raeder> abtaststrahlen(const Kennwerte& kennwerte,
                                                       const Karosseriebewegung& karosserie) {
    const Vec3 unten = drehe(karosserie.ausrichtung, {0.0, 0.0, -1.0});
    std::array<Abtaststrahl, anzahl_raeder> strahlen;
    const auto aufhaengungen = radaufhaengungen(kennwerte);
    for (int i = 0; i < anzahl_raeder; ++i) {
        const Radaufhaengung& r = aufhaengungen[i];
        strahlen[i] = {karosserie.position + drehe(karosserie.ausrichtung, r.anlenkpunkt), unten,
                       r.achse->fahrwerk.federweg + r.achse->radradius};
    }
    return strahlen;
}

SchrittErgebnis fahrzeugschritt(const Zustand& zustand, const Kennwerte& kennwerte,
                                const EingabePaket& eingabe, const Radkontakte& kontakte,
                                const Karosseriebewegung& karosserie, double dt) {
    SchrittErgebnis ergebnis;
    ergebnis.zustand = zustand;

    const auto strahlen = abtaststrahlen(kennwerte, karosserie);
    const auto aufhaengungen = radaufhaengungen(kennwerte);
    const Vec3 vorn = drehe(karosserie.ausrichtung, {1.0, 0.0, 0.0});
    std::array<Radlage, anzahl_raeder> raeder;
    for (int i = 0; i < anzahl_raeder; ++i) {
        Radlage& rad = raeder[i];
        rad.achse = aufhaengungen[i].achse;
        const bool hinten = i >= hinten_links;
        const double achsanteil = hinten ? 1.0 - kennwerte.bremsen.balance : kennwerte.bremsen.balance;
        rad.bremsmoment = eingabe.bremse * kennwerte.bremsen.moment * achsanteil / 2.0;

        const Radkontakt& kontakt = kontakte[i];
        if (!kontakt.kontakt) continue;

        const Abtaststrahl& strahl = strahlen[i];
        const double abstand = skalar(kontakt.punkt - strahl.start, strahl.richtung);
        const double einfederung = std::max(strahl.laenge - abstand, 0.0);

        // Wie schnell sich der Anlenkpunkt dem Boden entlang des Strahls nähert.
        const Vec3 anlenkhebel = strahl.start - karosserie.position;
        const Vec3 v_anlenkpunkt =
            karosserie.geschwindigkeit + kreuz(karosserie.drehgeschwindigkeit, anlenkhebel);
        const double neigung = skalar(-strahl.richtung, kontakt.normale);
        const double einfedergeschwindigkeit = -skalar(v_anlenkpunkt, kontakt.normale) / neigung;

        // Die Radlast wirkt entlang der Bodennormalen im Kontaktpunkt.
        const double radlast = federkraft(rad.achse->fahrwerk, einfederung, einfedergeschwindigkeit);
        const Vec3 kraft = kontakt.normale * radlast;
        rad.kontakt = true;
        rad.hebel = kontakt.punkt - karosserie.position;
        ergebnis.kraefte.kraft += kraft;
        ergebnis.kraefte.moment += kreuz(rad.hebel, kraft);

        const Vec3 quer = kreuz(kontakt.normale, vorn);
        rad.laengs = kreuz(quer, kontakt.normale) * (1.0 / betrag(quer));
        rad.v_kontakt = karosserie.geschwindigkeit + kreuz(karosserie.drehgeschwindigkeit, rad.hebel);
        rad.haftkraft = radlast * reibwert(rad.achse->reifen, radlast, kontakt.reibwert);

        ergebnis.animationsdaten.federweg[i] = einfederung;
        ergebnis.telemetrie.radlast[i] = radlast;
    }

    // Räder und Reifen in Zwischenschritten. Radlast, Kontaktpunkt und Drehung der
    // Karosserie bleiben dabei fest. Bewusste Näherung: Nur die Geschwindigkeit der
    // Karosserie wird mit den Reifenkräften mitgeführt, damit der steife Reifen bei
    // kleinem Schlupfnenner nicht über den ganzen Tick gegen eine veraltete
    // Geschwindigkeit arbeitet (GTO/research/fahrphysik.md, 9.2).
    // Gangwechsel: Der gewünschte Gang greift erst nach der Schaltzeit, bis dahin
    // gibt es keinen Kraftschluss. Den Rückwärtsgang gibt es noch nicht.
    const Antrieb& antrieb = kennwerte.antrieb;
    const int gaenge = static_cast<int>(antrieb.getriebe.uebersetzungen.size());
    const int gangwunsch = std::clamp(static_cast<int>(eingabe.gangwunsch), 0, gaenge);
    if (gangwunsch != ergebnis.zustand.gang) {
        ergebnis.zustand.gang = gangwunsch;
        ergebnis.zustand.schaltrest = antrieb.getriebe.schaltzeit;
    }
    const double kupplungsmoment = antrieb.kupplung.moment * (1.0 - eingabe.kupplung); // N·m
    // Gesamtübersetzung vom Motor zur Hinterachse; das offene Differenzial gibt
    // beiden Hinterrädern dasselbe Moment.
    const double uebersetzung = ergebnis.zustand.gang > 0
                                    ? antrieb.getriebe.uebersetzungen[ergebnis.zustand.gang - 1] *
                                          antrieb.getriebe.achsuebersetzung
                                    : 0.0;
    const double radtraegheit_hinten = kennwerte.hinten.reifen.traegheit;
    double& motordrehzahl = ergebnis.zustand.motordrehzahl;

    const double h = dt / zwischenschritte;
    const double gewicht_zwischenschritt = 1.0 / zwischenschritte;
    Vec3 dv_karosserie;
    for (int schritt = 0; schritt < zwischenschritte; ++schritt) {
        motordrehzahl += h * motormoment(antrieb.motor, motordrehzahl, eingabe.gas) / antrieb.motor.traegheit;
        motordrehzahl = std::max(motordrehzahl, 0.0);

        Vec3 reifenkraft_gesamt;
        std::array<double, anzahl_raeder> drehzahl_frei{}; // rad/s nach Reifen und Antrieb, vor der Bremse
        for (int i = 0; i < anzahl_raeder; ++i) {
            const Radlage& rad = raeder[i];
            const Reifen& reifen = rad.achse->reifen;
            const double r = rad.achse->radradius;
            const double omega = ergebnis.zustand.raddrehzahl[i];

            double fx = 0.0;
            double dfx_domega = 0.0;
            if (rad.kontakt) {
                const double vx = skalar(rad.v_kontakt + dv_karosserie, rad.laengs);
                const double nenner = schlupfnenner(vx);
                const double schlupf = (omega * r - vx) / nenner;
                const Kurvenpunkt kurve = magic_formula(reifen.laengs, schlupf);
                fx = rad.haftkraft * kurve.wert;
                dfx_domega = rad.haftkraft * std::max(kurve.steigung, 0.0) * r / nenner;
            }

            // Halbimplizit: Die Reifenkraft wird um die neue Raddrehzahl linearisiert,
            // sonst schwingt das Rad bei kleinem Schlupfnenner.
            drehzahl_frei[i] = omega - h * fx * r / reifen.traegheit / (1.0 + h * r * dfx_domega / reifen.traegheit);
            fx = std::clamp(fx + dfx_domega * (drehzahl_frei[i] - omega), -rad.haftkraft, rad.haftkraft);

            if (!rad.kontakt) continue;
            const Vec3 kraft = rad.laengs * fx;
            reifenkraft_gesamt += kraft;
            ergebnis.kraefte.kraft += kraft * gewicht_zwischenschritt;
            ergebnis.kraefte.moment += kreuz(rad.hebel, kraft) * gewicht_zwischenschritt;
        }
        dv_karosserie += reifenkraft_gesamt * (h / kennwerte.masse);

        // Die Kupplung ist ein begrenzter Impuls, der Motor und Getriebeeingang
        // auf gleiche Drehzahl bringt: Reicht ihr Moment, haftet sie, sonst rutscht sie.
        ergebnis.zustand.schaltrest = std::max(ergebnis.zustand.schaltrest - h, 0.0);
        if (uebersetzung > 0.0 && ergebnis.zustand.schaltrest == 0.0) {
            const double eingang =
                uebersetzung * (drehzahl_frei[hinten_links] + drehzahl_frei[hinten_rechts]) / 2.0;
            const double kehrwert_traegheit =
                1.0 / antrieb.motor.traegheit + uebersetzung * uebersetzung / (2.0 * radtraegheit_hinten);
            const double grenze = kupplungsmoment * h; // N·m·s
            const double impuls = std::clamp((motordrehzahl - eingang) / kehrwert_traegheit, -grenze, grenze);
            motordrehzahl -= impuls / antrieb.motor.traegheit;
            for (int i : {hinten_links, hinten_rechts}) {
                drehzahl_frei[i] += impuls * uebersetzung / (2.0 * radtraegheit_hinten);
            }
        }

        // Die Bremse ist ein begrenzter Impuls Richtung Stillstand: Reicht er,
        // steht das Rad, sonst nimmt er seinen vollen Betrag weg.
        for (int i = 0; i < anzahl_raeder; ++i) {
            const double bremsimpuls = raeder[i].bremsmoment * h / raeder[i].achse->reifen.traegheit;
            const double frei = drehzahl_frei[i];
            ergebnis.zustand.raddrehzahl[i] =
                std::abs(frei) <= bremsimpuls ? 0.0 : frei - std::copysign(bremsimpuls, frei);
        }
    }
    ergebnis.animationsdaten.drehzahl = ergebnis.zustand.motordrehzahl;
    return ergebnis;
}

} // namespace ultragravity
