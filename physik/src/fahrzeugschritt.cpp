#include "ultragravity/fahrzeugschritt.h"

#include <algorithm>
#include <cmath>
#include <numbers>

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

bool vorderrad(int rad) { return rad < hinten_links; }

// Das andere Rad derselben Achse.
int gegenueber(int rad) { return rad ^ 1; }

// Kraft von Feder, Anschlag und Dämpfer auf ein Rad.
double federkraft(const Fahrwerk& f, double einfederung, double einfedergeschwindigkeit) {
    double kraft = f.federrate * einfederung;
    if (einfederung > f.federweg) kraft += f.anschlagrate * (einfederung - f.federweg);
    const double daempfung = einfedergeschwindigkeit > 0.0 ? f.daempfer_druck : f.daempfer_zug;
    kraft += daempfung * einfedergeschwindigkeit;
    // Der Boden kann nur drücken, nicht ziehen.
    return std::max(kraft, 0.0);
}

// Last, die der Stabilisator dem Rad zusätzlich gibt und seinem Gegenüber nimmt:
// mehr dem weiter eingefederten. Er kann nur verschieben, was das entlastete Rad
// trägt; hebt es ab, stützt er sich gegen nichts mehr ab.
double stabilisatorlast(const Fahrwerk& f, double einfederung, double einfederung_gegenueber, double last,
                        double last_gegenueber) {
    return std::clamp(f.stabilisator * (einfederung - einfederung_gegenueber), -last, last_gegenueber);
}

// Unterhalb dieser Geschwindigkeit geht der Schlupfnenner glatt in einen festen
// Wert über, damit der Schlupf bei Stillstand nicht gegen unendlich läuft.
// Zwischenschritte für Räder und Reifen je Fahrzeug-Schritt (ADR-0006).
constexpr int zwischenschritte = 8;

// Drehung des Lenkrads bei vollem Ausschlag, je Seite (450°).
constexpr double lenkrad_ausschlag = 2.5 * std::numbers::pi; // rad

constexpr double schlupfnenner_minimum = 1.0; // m/s

// Untergrenze der Radgeschwindigkeit für die Relaxation. Ohne sie fröre der
// Schräglauf im Stand ein, weil sich der Reifen nur beim Rollen verformt.
constexpr double relaxation_minimum = 1.0; // m/s

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

// Gewichtungsfunktion für kombinierten Schlupf: 1 ohne den anderen Schlupf,
// fällt mit ihm gegen null.
double gewichtung(double b, double schlupf) { return 1.0 / std::sqrt(1.0 + b * b * schlupf * schlupf); }

// Reibwert zwischen Reifen und Belag. Mehr Radlast bringt unterproportional
// mehr Kraft (Lastempfindlichkeit).
double reibwert(const Reifen& r, double radlast, double belag) {
    const double mehrlast = (radlast - r.nennlast) / r.nennlast;
    return std::max(belag * r.spitzenreibwert * (1.0 - r.lastempfindlichkeit * mehrlast), 0.0);
}

// Unterhalb der Leerlaufdrehzahl gibt der Leerlaufregler über diesen Bereich
// hinweg von null bis Vollgas.
constexpr double leerlauf_regelbereich = 100.0 * 2.0 * std::numbers::pi / 60.0; // rad/s

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
    Vec3 hebel;                  // m, vom Schwerpunkt zum Kontaktpunkt, Weltsystem
    Vec3 laengs;                 // 1, Rollrichtung in der Bodenebene
    Vec3 quer;                   // 1, in der Bodenebene nach links, quer zur Rollrichtung
    Vec3 v_kontakt;              // m/s, Geschwindigkeit der Karosserie im Kontaktpunkt
    double radlast = 0.0;        // N
    double haftkraft = 0.0;      // N, Radlast mal Reibwert
    double bremsmoment = 0.0;    // N·m
};

// Wie weit ein Rad mit Kontakt eingefedert ist und wie schnell es weiter einfedert.
struct Federung {
    double einfederung = 0.0;  // m
    double geschwindigkeit = 0.0; // m/s, positiv beim Einfedern
};

Federung federung(const Abtaststrahl& strahl, const Radkontakt& kontakt, const Karosseriebewegung& karosserie) {
    const double abstand = skalar(kontakt.punkt - strahl.start, strahl.richtung); // m
    // Wie schnell sich der Anlenkpunkt dem Boden entlang des Strahls nähert.
    const Vec3 anlenkhebel = strahl.start - karosserie.position;
    const Vec3 v_anlenkpunkt = karosserie.geschwindigkeit + kreuz(karosserie.drehgeschwindigkeit, anlenkhebel);
    const double neigung = skalar(-strahl.richtung, kontakt.normale); // 1
    return {std::max(strahl.laenge - abstand, 0.0), -skalar(v_anlenkpunkt, kontakt.normale) / neigung};
}

// Rollrichtung und Querrichtung eines Rads in der Bodenebene: Sie folgen der
// Karosserie, um den Lenkwinkel gedreht.
void richte_aus(Radlage& rad, Vec3 normale, Vec3 vorn, double lenkwinkel) {
    const Vec3 quer = kreuz(normale, vorn);
    const Vec3 laengs = kreuz(quer, normale) * (1.0 / betrag(quer));
    const Vec3 links = kreuz(normale, laengs);
    rad.laengs = laengs * std::cos(lenkwinkel) + links * std::sin(lenkwinkel);
    rad.quer = kreuz(normale, rad.laengs);
}

// Kräfte eines Reifens in einem Zwischenschritt.
struct Reifenkraft {
    double laengs = 0.0;     // N
    double quer = 0.0;       // N
    double steigung = 0.0;   // N·s/rad, Ableitung der Längskraft nach der Raddrehzahl
    double grenze = 0.0;     // N, was der Schräglauf an Längskraft übrig lässt
};

// Reifenkräfte eines Rads mit Kontakt bei der Geschwindigkeit v im Kontaktpunkt.
// Der Schräglauf, den der Reifen spürt, folgt dem kinematischen über die
// Relaxationslänge: dα/ds = (α_kin − α)/σ mit s der Strecke, die das Rad über
// den Boden zurücklegt, exakt über den Zwischenschritt.
Reifenkraft reifenkraft(const Radlage& rad, Vec3 v, double omega, double h, double& schraeglauf) {
    const Reifen& reifen = rad.achse->reifen;
    const double r = rad.achse->radradius;          // m
    const double vx = skalar(v, rad.laengs);        // m/s
    const double vy = skalar(v, rad.quer);          // m/s
    const double nenner = schlupfnenner(vx);        // m/s
    const double kinematisch = std::atan(vy / nenner); // rad
    const double weg = std::max(std::hypot(vx, vy), relaxation_minimum) * h; // m
    schraeglauf += (kinematisch - schraeglauf) * (1.0 - std::exp(-weg / reifen.relaxationslaenge));

    const double schlupf = (omega * r - vx) / nenner; // 1
    const Kurvenpunkt kurve = magic_formula(reifen.laengs, schlupf);
    Reifenkraft k;
    k.grenze = rad.haftkraft * gewichtung(reifen.kombiniert.laengs, schraeglauf);
    k.laengs = k.grenze * kurve.wert;
    k.steigung = k.grenze * std::max(kurve.steigung, 0.0) * r / nenner;
    k.quer = -rad.haftkraft * gewichtung(reifen.kombiniert.quer, schlupf) * magic_formula(reifen.quer, schraeglauf).wert;
    return k;
}

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
    const double lenkwinkel_vorn = std::clamp(eingabe.lenkung, -1.0, 1.0) * kennwerte.lenkung.max_lenkwinkel; // rad

    // Federn und Dämpfer zuerst für alle Räder, weil der Stabilisator beide Räder
    // einer Achse braucht. Ohne Kontakt hängt ein Rad voll ausgefedert und lastlos.
    std::array<Federung, anzahl_raeder> federungen{};
    std::array<double, anzahl_raeder> federlast{}; // N
    for (int i = 0; i < anzahl_raeder; ++i) {
        if (!kontakte[i].kontakt) continue;
        federungen[i] = federung(strahlen[i], kontakte[i], karosserie);
        federlast[i] =
            federkraft(aufhaengungen[i].achse->fahrwerk, federungen[i].einfederung, federungen[i].geschwindigkeit);
    }

    std::array<Radlage, anzahl_raeder> raeder;
    for (int i = 0; i < anzahl_raeder; ++i) {
        Radlage& rad = raeder[i];
        rad.achse = aufhaengungen[i].achse;
        const double achsanteil = vorderrad(i) ? kennwerte.bremsen.balance : 1.0 - kennwerte.bremsen.balance;
        rad.bremsmoment = eingabe.bremse * kennwerte.bremsen.moment * achsanteil / 2.0;

        const Radkontakt& kontakt = kontakte[i];
        if (!kontakt.kontakt) {
            ergebnis.zustand.schraeglauf[i] = 0.0;
            continue;
        }

        // Die Radlast wirkt entlang der Bodennormalen im Kontaktpunkt.
        const int g = gegenueber(i);
        const double radlast = federlast[i] + stabilisatorlast(rad.achse->fahrwerk, federungen[i].einfederung,
                                                               federungen[g].einfederung, federlast[i],
                                                               federlast[g]); // N
        const Vec3 kraft = kontakt.normale * radlast;
        rad.kontakt = true;
        rad.hebel = kontakt.punkt - karosserie.position;
        ergebnis.kraefte.kraft += kraft;
        ergebnis.kraefte.moment += kreuz(rad.hebel, kraft);

        richte_aus(rad, kontakt.normale, vorn, vorderrad(i) ? lenkwinkel_vorn : 0.0);
        rad.v_kontakt = karosserie.geschwindigkeit + kreuz(karosserie.drehgeschwindigkeit, rad.hebel);
        rad.radlast = radlast;
        rad.haftkraft = radlast * reibwert(rad.achse->reifen, radlast, kontakt.reibwert);

        // Rollwiderstand: proportional zur Radlast, gegen die Rollrichtung. Der
        // glatte Vorzeichenübergang hält ihn im Stand bei null.
        const double vx = skalar(rad.v_kontakt, rad.laengs); // m/s
        const Vec3 rollkraft = rad.laengs * (-rad.achse->reifen.rollwiderstand * radlast * vx / schlupfnenner(vx));
        ergebnis.kraefte.kraft += rollkraft;
        ergebnis.kraefte.moment += kreuz(rad.hebel, rollkraft);

        ergebnis.animationsdaten.federweg[i] = federungen[i].einfederung;
        ergebnis.telemetrie.radlast[i] = radlast;
    }

    // Luft: Der Widerstand wirkt gegen die Fahrtrichtung im Schwerpunkt, der
    // Abtrieb je Achse senkrecht zur Karosserie nach unten an der Achsposition.
    const double staudruck_faktor = 0.5 * luftdichte; // kg/m³
    ergebnis.kraefte.kraft +=
        karosserie.geschwindigkeit * (-staudruck_faktor * kennwerte.aero.luftwiderstand * betrag(karosserie.geschwindigkeit));
    const double fahrtwind = std::max(skalar(karosserie.geschwindigkeit, vorn), 0.0); // m/s
    for (const Achse* achse : {&kennwerte.vorn, &kennwerte.hinten}) {
        const Vec3 abtrieb = drehe(karosserie.ausrichtung, {0.0, 0.0, -1.0}) *
                             (staudruck_faktor * achse->abtrieb * fahrtwind * fahrtwind); // N
        const Vec3 hebel = drehe(karosserie.ausrichtung, Vec3{achse->x, 0.0, 0.0} - kennwerte.schwerpunkt);
        ergebnis.kraefte.kraft += abtrieb;
        ergebnis.kraefte.moment += kreuz(hebel, abtrieb);
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
    Vec3 dv_karosserie; // m/s
    for (int schritt = 0; schritt < zwischenschritte; ++schritt) {
        motordrehzahl += h * motormoment(antrieb.motor, motordrehzahl, eingabe.gas) / antrieb.motor.traegheit;
        motordrehzahl = std::max(motordrehzahl, 0.0);
        for (int i = 0; i < anzahl_raeder; ++i) ergebnis.zustand.raddrehwinkel[i] += h * ergebnis.zustand.raddrehzahl[i];

        Vec3 reifenkraft_gesamt; // N
        std::array<double, anzahl_raeder> drehzahl_frei{}; // rad/s nach Reifen und Antrieb, vor der Bremse
        for (int i = 0; i < anzahl_raeder; ++i) {
            const Radlage& rad = raeder[i];
            const double traegheit = rad.achse->reifen.traegheit; // kg·m²
            const double r = rad.achse->radradius;                // m
            const double omega = ergebnis.zustand.raddrehzahl[i]; // rad/s
            const Reifenkraft k = rad.kontakt ? reifenkraft(rad, rad.v_kontakt + dv_karosserie, omega, h,
                                                            ergebnis.zustand.schraeglauf[i])
                                              : Reifenkraft{};

            // Halbimplizit: Die Reifenkraft wird um die neue Raddrehzahl linearisiert,
            // sonst schwingt das Rad bei kleinem Schlupfnenner.
            drehzahl_frei[i] = omega - h * k.laengs * r / traegheit / (1.0 + h * r * k.steigung / traegheit);
            const double fx = std::clamp(k.laengs + k.steigung * (drehzahl_frei[i] - omega), -k.grenze, k.grenze); // N

            if (!rad.kontakt) continue;
            const Vec3 kraft = rad.laengs * fx + rad.quer * k.quer; // N
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
    Animationsdaten& anim = ergebnis.animationsdaten;
    for (int i = 0; i < anzahl_raeder; ++i) {
        ergebnis.zustand.raddrehwinkel[i] = wickle_winkel(ergebnis.zustand.raddrehwinkel[i]);
        anim.raddrehwinkel[i] = ergebnis.zustand.raddrehwinkel[i];
        anim.lenkwinkel[i] = vorderrad(i) ? lenkwinkel_vorn : 0.0;
    }
    // Aus der Ausrichtung der Karosserie: Nicken aus der Neigung der Längsachse,
    // Wanken aus der Neigung der Querachse.
    anim.nicken = std::asin(std::clamp(-vorn.z, -1.0, 1.0));
    anim.wanken = std::asin(std::clamp(drehe(karosserie.ausrichtung, {0.0, 1.0, 0.0}).z, -1.0, 1.0));
    anim.lenkrad = std::clamp(eingabe.lenkung, -1.0, 1.0) * lenkrad_ausschlag;
    anim.drehzahl = ergebnis.zustand.motordrehzahl;
    return ergebnis;
}

} // namespace ultragravity
