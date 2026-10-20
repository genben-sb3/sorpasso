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

// Dichte der Luft, aus der Luftwiderstand und Abtrieb ihre Kraft ziehen.
constexpr double luftdichte = 1.2; // kg/m³

// Luft: Der Widerstand wirkt gegen die Fahrtrichtung im Schwerpunkt, der
// Abtrieb je Achse senkrecht zur Karosserie nach unten an der Achsposition.
void luftkraefte(const Kennwerte& kennwerte, const Karosseriebewegung& karosserie, const Vec3& vorn, Kraefte& kraefte) {
    const double staudruck_faktor = 0.5 * luftdichte; // kg/m³
    kraefte.kraft +=
        karosserie.geschwindigkeit * (-staudruck_faktor * kennwerte.aero.luftwiderstand * betrag(karosserie.geschwindigkeit));
    const double fahrtwind = std::max(skalar(karosserie.geschwindigkeit, vorn), 0.0); // m/s
    for (const Achse* achse : {&kennwerte.vorn, &kennwerte.hinten}) {
        const Vec3 abtrieb = drehe(karosserie.ausrichtung, {0.0, 0.0, -1.0}) *
                             (staudruck_faktor * achse->abtrieb * fahrtwind * fahrtwind); // N
        const Vec3 hebel = drehe(karosserie.ausrichtung, Vec3{achse->x, 0.0, 0.0} - kennwerte.schwerpunkt);
        kraefte.kraft += abtrieb;
        kraefte.moment += kreuz(hebel, abtrieb);
    }
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

// Längsschlupf eines Rads mit Drehzahl omega (rad/s) und Radradius r (m) bei der
// Längsgeschwindigkeit v (m/s) im Kontaktpunkt. Einheitenlos.
double laengsschlupf(double omega, double r, double v) { return (omega * r - v) / schlupfnenner(v); }

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
    double schlupf = 0.0;    // 1, Längsschlupf
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

    const double schlupf = laengsschlupf(omega, r, vx); // 1
    const Kurvenpunkt kurve = magic_formula(reifen.laengs, schlupf);
    Reifenkraft k;
    k.schlupf = schlupf;
    k.grenze = rad.haftkraft * gewichtung(reifen.kombiniert.laengs, schraeglauf);
    k.laengs = k.grenze * kurve.wert;
    k.steigung = k.grenze * std::max(kurve.steigung, 0.0) * r / nenner;
    k.quer = -rad.haftkraft * gewichtung(reifen.kombiniert.quer, schlupf) * magic_formula(reifen.quer, schraeglauf).wert;
    return k;
}

// Eine angetriebene Achse samt ihrem Anteil am Antriebsmoment.
struct Antriebsachse {
    int links;
    int rechts;
    const Achse* achse;
    double anteil; // 1
};

struct Antriebsachsen {
    std::array<Antriebsachse, 2> achsen{};
    int anzahl = 0;
};

Antriebsachsen antriebsachsen(const Kennwerte& k) {
    const Antriebsachse vorn{vorn_links, vorn_rechts, &k.vorn, 1.0};
    const Antriebsachse hinten{hinten_links, hinten_rechts, &k.hinten, 1.0};
    const Antrieb& a = k.antrieb;
    switch (a.art) {
    case Antriebsart::heck: return {{hinten, {}}, 1};
    case Antriebsart::front: return {{vorn, {}}, 1};
    case Antriebsart::allrad: break;
    }
    Antriebsachse v = vorn;
    Antriebsachse h = hinten;
    v.anteil = a.momentverteilung_vorn;
    h.anteil = 1.0 - a.momentverteilung_vorn;
    return {{v, h}, 2};
}

// Mittlerer Radradius der angetriebenen Achsen, gewichtet mit ihrem Anteil.
double antriebsradius(const Antriebsachsen& angetrieben) {
    double radius = 0.0; // m
    for (int i = 0; i < angetrieben.anzahl; ++i) {
        radius += angetrieben.achsen[i].anteil * angetrieben.achsen[i].achse->radradius;
    }
    return radius;
}

// Gewichtete mittlere Raddrehzahl der angetriebenen Achsen.
double eingangsdrehzahl(const Antriebsachsen& angetrieben, const std::array<double, anzahl_raeder>& drehzahl) {
    double summe = 0.0; // rad/s
    for (int i = 0; i < angetrieben.anzahl; ++i) {
        const Antriebsachse& a = angetrieben.achsen[i];
        summe += a.anteil * (drehzahl[a.links] + drehzahl[a.rechts]) / 2.0;
    }
    return summe;
}

// Ein Impuls am Getriebeeingang (übersetzt) verteilt sich auf die Räder der angetriebenen Achsen.
void verteile_impuls(const Antriebsachsen& angetrieben, double impuls, // N·m·s am Getriebeeingang
                     double uebersetzung, std::array<double, anzahl_raeder>& drehzahl) {
    for (int i = 0; i < angetrieben.anzahl; ++i) {
        const Antriebsachse& a = angetrieben.achsen[i];
        const double dw = impuls * uebersetzung * a.anteil / (2.0 * a.achse->reifen.traegheit); // rad/s
        drehzahl[a.links] += dw;
        drehzahl[a.rechts] += dw;
    }
}

// Begrenzter Impuls, der die Differenz zweier Drehzahlen angleicht: Er ist der
// volle Ausgleich, höchstens aber +-grenze.
double angleichimpuls(double unterschied, // rad/s
                      double traegheit,   // kg·m², wirksame Trägheit des Ausgleichs
                      double grenze) {    // N·m·s
    return std::clamp(unterschied * traegheit, -grenze, grenze);
}

double gesamtuebersetzung(const Antrieb& a, int gang) {
    return gang > 0 ? a.getriebe.uebersetzungen[gang - 1] * a.getriebe.achsuebersetzung : 0.0;
}

// Der Gang, den die Automatik als nächsten wählt. Sie rechnet mit der Drehzahl,
// die zur Geschwindigkeit über Grund gehört: Durchdrehende Räder treiben sie
// nicht in den nächsten Gang. Im Drift hält sie den Gang.
int automatikgang(const Zustand& zustand, const Kennwerte& k, double bremse, Vec3 geschwindigkeit_fahrzeug,
                  double radius) {
    const Antrieb& a = k.antrieb;
    const Automatik& au = a.getriebe.automatik;
    const int gaenge = static_cast<int>(a.getriebe.uebersetzungen.size());
    const int gang = zustand.gang == 0 ? 1 : zustand.gang;
    if (zustand.gang == 0) return gang;
    if (zustand.gangalter < au.haltezeit) return gang;

    constexpr double bremsschwelle = 0.05; // 1, Bremseingabe ab der die Automatik als bremsend gilt
    const bool bremst = bremse >= bremsschwelle;
    const double vx = geschwindigkeit_fahrzeug.x; // m/s
    const double vy = geschwindigkeit_fahrzeug.y; // m/s
    constexpr double driftgeschwindigkeit = 2.0;  // m/s, darunter gibt es keinen Driftwinkel
    if (std::hypot(vx, vy) > driftgeschwindigkeit && std::atan2(std::abs(vy), std::abs(vx)) > au.driftwinkel) {
        return gang;
    }

    const auto bodendrehzahl = [&](int g) { return std::abs(vx) / radius * gesamtuebersetzung(a, g); }; // rad/s
    if (gang < gaenge && !bremst && bodendrehzahl(gang) > au.hochschaltdrehzahl) return gang + 1;
    const double runter = bremst ? au.bremsschaltdrehzahl : au.runterschaltdrehzahl; // rad/s
    if (gang > 1 && bodendrehzahl(gang) < runter && bodendrehzahl(gang - 1) < au.hochschaltdrehzahl) {
        return gang - 1;
    }
    return gang;
}

// Traktionskontrolle: Sie nimmt dem Fahrer Gas weg, solange der Schlupf der
// Antriebsräder über dem Ziel der Stufe liegt, und gibt es erst wieder, wenn er
// deutlich darunter liegt. Dazwischen hält sie den Faktor (Hysterese), damit sie
// nicht an- und ausschaltet. Stufe 0 greift nie ein.
constexpr double tk_ziel_stufe0 = 0.30;       // 1, Ziel-Schlupf, von dem aus die Stufen abziehen
constexpr int    tk_max_stufe = 5;            // höchste Stufe, größere werden begrenzt
constexpr double tk_ziel_minimum = 0.01;      // 1, Untergrenze des Ziel-Schlupfs
constexpr double tk_ziel_je_stufe = 0.05;     // 1, weniger Ziel-Schlupf je Stufe
constexpr double tk_untere_schwelle = 0.8;    // 1, Anteil des Ziels, unter dem das Gas zurückkommt
constexpr double tk_abregeln = 2.0;           // 1/s, Gasfaktor, der je Sekunde wegfällt
constexpr double tk_aufregeln = 0.5;          // 1/s, Gasfaktor, der je Sekunde zurückkommt

double traktionsfaktor(double faktor, int stufe, const Antriebsachsen& angetrieben,
                       const std::array<double, anzahl_raeder>& raddrehzahl, double v_laengs, double dt) {
    if (stufe <= 0) return 1.0;
    const double ziel = std::max(tk_ziel_stufe0 - tk_ziel_je_stufe * std::min(stufe, tk_max_stufe), tk_ziel_minimum); // 1
    double schlupf = 0.0; // 1, größter Antriebsschlupf
    for (int i = 0; i < angetrieben.anzahl; ++i) {
        const Antriebsachse& a = angetrieben.achsen[i];
        for (int rad : {a.links, a.rechts}) {
            schlupf = std::max(schlupf, laengsschlupf(raddrehzahl[rad], a.achse->radradius, v_laengs));
        }
    }
    if (schlupf > ziel) return std::max(faktor - tk_abregeln * dt, 0.0);
    if (schlupf < tk_untere_schwelle * ziel) return std::min(faktor + tk_aufregeln * dt, 1.0);
    return faktor;
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

    luftkraefte(kennwerte, karosserie, vorn, ergebnis.kraefte);

    // Räder und Reifen in Zwischenschritten. Radlast, Kontaktpunkt und Drehung der
    // Karosserie bleiben dabei fest. Bewusste Näherung: Nur die Geschwindigkeit der
    // Karosserie wird mit den Reifenkräften mitgeführt, damit der steife Reifen bei
    // kleinem Schlupfnenner nicht über den ganzen Tick gegen eine veraltete
    // Geschwindigkeit arbeitet (GTO/research/fahrphysik.md, 9.2).
    const Antrieb& antrieb = kennwerte.antrieb;
    const Antriebsachsen angetrieben = antriebsachsen(kennwerte);
    const bool automatik = antrieb.getriebe.modus == Getriebemodus::automatik;
    const int gaenge = static_cast<int>(antrieb.getriebe.uebersetzungen.size());
    ergebnis.zustand.gangalter += dt;
    int gangwunsch = std::clamp(static_cast<int>(eingabe.gangwunsch), 0, gaenge);
    if (automatik) {
        // Die Automatik kennt nur Leerlauf (0) und Fahren (alles darüber).
        const Vec3 v_fahrzeug{skalar(karosserie.geschwindigkeit, vorn),
                              skalar(karosserie.geschwindigkeit, drehe(karosserie.ausrichtung, {0.0, 1.0, 0.0})),
                              0.0};
        gangwunsch = eingabe.gangwunsch > 0
                         ? automatikgang(zustand, kennwerte, eingabe.bremse, v_fahrzeug, antriebsradius(angetrieben))
                         : 0;
    }
    // Gangwechsel: Der gewünschte Gang greift erst nach der Schaltzeit, bis dahin
    // gibt es keinen Kraftschluss. Den Rückwärtsgang gibt es noch nicht.
    if (gangwunsch != ergebnis.zustand.gang) {
        ergebnis.zustand.gang = gangwunsch;
        ergebnis.zustand.schaltrest = antrieb.getriebe.schaltzeit;
        ergebnis.zustand.gangalter = 0.0;
    }
    const double uebersetzung = gesamtuebersetzung(antrieb, ergebnis.zustand.gang);
    double& motordrehzahl = ergebnis.zustand.motordrehzahl;
    ergebnis.zustand.traktionsfaktor =
        traktionsfaktor(zustand.traktionsfaktor, eingabe.fahrhilfen.traktionskontrolle, angetrieben,
                        zustand.raddrehzahl, skalar(karosserie.geschwindigkeit, vorn), dt);
    // Die Automatik kuppelt selbst: mit der Motordrehzahl über Leerlauf ein und
    // nach dem Schalten innerhalb der Einkuppelzeit wieder.
    const Automatik& au = antrieb.getriebe.automatik;
    const double nach_schalten =
        automatik ? std::clamp((ergebnis.zustand.gangalter - antrieb.getriebe.schaltzeit) / std::max(au.einkuppelzeit, dt), 0.0, 1.0)
                  : 1.0; // 1
    double traegheit_kehrwert_raeder = 0.0; // 1/(kg·m²), Räder aus Sicht eines Impulses am Getriebeeingang
    for (int i = 0; i < angetrieben.anzahl; ++i) {
        const Antriebsachse& a = angetrieben.achsen[i];
        traegheit_kehrwert_raeder += a.anteil * a.anteil / (2.0 * a.achse->reifen.traegheit);
    }

    const double h = dt / zwischenschritte;
    const double gewicht_zwischenschritt = 1.0 / zwischenschritte;
    Vec3 dv_karosserie; // m/s
    for (int schritt = 0; schritt < zwischenschritte; ++schritt) {
        // Beim automatischen Schalten nimmt die Automatik das Gas weg.
        double gas = automatik && ergebnis.zustand.schaltrest > 0.0 ? 0.0 : eingabe.gas;
        gas *= ergebnis.zustand.traktionsfaktor;
        motordrehzahl += h * motormoment(antrieb.motor, motordrehzahl, gas) / antrieb.motor.traegheit;
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
            ergebnis.telemetrie.schlupf[i] += k.schlupf * gewicht_zwischenschritt;
            ergebnis.telemetrie.kraft_laengs[i] += fx * gewicht_zwischenschritt;
            ergebnis.telemetrie.kraft_quer[i] += k.quer * gewicht_zwischenschritt;
            ergebnis.kraefte.kraft += kraft * gewicht_zwischenschritt;
            ergebnis.kraefte.moment += kreuz(rad.hebel, kraft) * gewicht_zwischenschritt;
        }
        dv_karosserie += reifenkraft_gesamt * (h / kennwerte.masse);

        // Die Kupplung ist ein begrenzter Impuls, der Motor und Getriebeeingang
        // auf gleiche Drehzahl bringt: Reicht ihr Moment, haftet sie, sonst rutscht sie.
        ergebnis.zustand.schaltrest = std::max(ergebnis.zustand.schaltrest - h, 0.0);
        double antriebsimpuls = 0.0; // N·m·s am Getriebeeingang
        if (uebersetzung > 0.0 && ergebnis.zustand.schaltrest == 0.0) {
            const double kupplungsgrad =
                automatik ? std::clamp((motordrehzahl - antrieb.motor.leerlaufdrehzahl) / au.einkuppelbereich, 0.0, 1.0) *
                                nach_schalten
                          : 1.0 - eingabe.kupplung; // 1
            const double eingang = eingangsdrehzahl(angetrieben, drehzahl_frei) * uebersetzung; // rad/s
            const double kehrwert_traegheit =
                1.0 / antrieb.motor.traegheit + uebersetzung * uebersetzung * traegheit_kehrwert_raeder;
            const double grenze = antrieb.kupplung.moment * kupplungsgrad * h; // N·m·s
            antriebsimpuls = std::clamp((motordrehzahl - eingang) / kehrwert_traegheit, -grenze, grenze);
            motordrehzahl -= antriebsimpuls / antrieb.motor.traegheit;
            verteile_impuls(angetrieben, antriebsimpuls, uebersetzung, drehzahl_frei);
        }

        // Das Sperrdifferenzial überträgt Moment vom schnelleren auf das langsamere
        // Rad, höchstens bis zum Sperrmoment. Wie die Kupplung ist das ein begrenzter
        // Impuls, der die Drehzahlen angleicht.
        if (antrieb.differenzial.sperrt) {
            for (int i = 0; i < angetrieben.anzahl; ++i) {
                const Antriebsachse& a = angetrieben.achsen[i];
                const double achsmoment = std::abs(antriebsimpuls) / h * uebersetzung * a.anteil; // N·m
                const double sperrmoment =
                    antrieb.differenzial.vorspannung + antrieb.differenzial.sperrwirkung * achsmoment; // N·m
                const double traegheit = a.achse->reifen.traegheit; // kg·m²
                const double grenze = sperrmoment * h;               // N·m·s
                const double unterschied = drehzahl_frei[a.links] - drehzahl_frei[a.rechts]; // rad/s
                const double impuls = angleichimpuls(unterschied, traegheit / 2.0, grenze);  // N·m·s
                drehzahl_frei[a.links] -= impuls / traegheit;
                drehzahl_frei[a.rechts] += impuls / traegheit;
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
    ergebnis.telemetrie.schraeglauf = ergebnis.zustand.schraeglauf;
    ergebnis.telemetrie.geschwindigkeit = skalar(karosserie.geschwindigkeit, vorn);
    return ergebnis;
}

} // namespace ultragravity
