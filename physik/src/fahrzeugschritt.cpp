#include "ultragravity/fahrzeugschritt.h"

#include <algorithm>

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

} // namespace

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
                                const EingabePaket& /*eingabe*/, const Radkontakte& kontakte,
                                const Karosseriebewegung& karosserie, double /*dt*/) {
    SchrittErgebnis ergebnis;
    ergebnis.zustand = zustand;

    const auto strahlen = abtaststrahlen(kennwerte, karosserie);
    const auto aufhaengungen = radaufhaengungen(kennwerte);
    for (int i = 0; i < anzahl_raeder; ++i) {
        const Radkontakt& kontakt = kontakte[i];
        if (!kontakt.kontakt) continue;

        const Abtaststrahl& strahl = strahlen[i];
        const double abstand = skalar(kontakt.punkt - strahl.start, strahl.richtung);
        const double einfederung = std::max(strahl.laenge - abstand, 0.0);

        // Wie schnell sich der Anlenkpunkt dem Boden entlang des Strahls nähert.
        const Vec3 hebel = strahl.start - karosserie.position;
        const Vec3 v_anlenkpunkt =
            karosserie.geschwindigkeit + kreuz(karosserie.drehgeschwindigkeit, hebel);
        const double neigung = skalar(-strahl.richtung, kontakt.normale);
        const double einfedergeschwindigkeit = -skalar(v_anlenkpunkt, kontakt.normale) / neigung;

        // Die Radlast wirkt entlang der Bodennormalen im Kontaktpunkt.
        const double radlast =
            federkraft(aufhaengungen[i].achse->fahrwerk, einfederung, einfedergeschwindigkeit);
        const Vec3 kraft = kontakt.normale * radlast;
        ergebnis.kraefte.kraft += kraft;
        ergebnis.kraefte.moment += kreuz(kontakt.punkt - karosserie.position, kraft);

        ergebnis.animationsdaten.federweg[i] = einfederung;
        ergebnis.telemetrie.radlast[i] = radlast;
    }
    return ergebnis;
}

} // namespace ultragravity
