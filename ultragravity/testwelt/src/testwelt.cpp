#include "ultragravity/testwelt.h"

#include <algorithm>
#include <limits>

#include "ultragravity/pruefsumme.h"

namespace ultragravity {

Radkontakt Testgelaende::taste_ab(const Abtaststrahl& strahl) const {
    if (strahl.richtung.z >= 0.0) return {};
    // Liegt der Anlenkpunkt schon unter dem Boden, zählt der Strahl ab seinem Start.
    const double weg = std::max(strahl.start.z / -strahl.richtung.z, 0.0);
    if (weg > strahl.laenge) return {};
    const Vec3 punkt = strahl.start + strahl.richtung * weg;
    double reibwert_hier = reibwert;
    for (const Belagzone& zone : belagzonen) {
        if (punkt.x >= zone.x_von && punkt.x < zone.x_bis && punkt.y >= zone.y_von && punkt.y < zone.y_bis) {
            reibwert_hier = zone.reibwert;
            break;
        }
    }
    return {true, punkt, {0.0, 0.0, 1.0}, reibwert_hier};
}

Testwelt::Testwelt(Kennwerte kennwerte, double absetzhoehe, Testgelaende gelaende, double geschwindigkeit)
    : kennwerte_(std::move(kennwerte)), gelaende_(std::move(gelaende)) {
    double tiefster_punkt = std::numeric_limits<double>::infinity();
    for (const Abtaststrahl& s : abtaststrahlen(kennwerte_, karosserie_)) {
        tiefster_punkt = std::min(tiefster_punkt, (s.start + s.richtung * s.laenge).z);
    }
    karosserie_.position.z = absetzhoehe - tiefster_punkt;
    karosserie_.geschwindigkeit.x = geschwindigkeit;
    zustand_ = rollender_zustand(kennwerte_, geschwindigkeit);
}

void Testwelt::tick(const EingabePaket& eingabe) {
    const auto strahlen = abtaststrahlen(kennwerte_, karosserie_);
    Radkontakte kontakte;
    for (int i = 0; i < anzahl_raeder; ++i) kontakte[i] = gelaende_.taste_ab(strahlen[i]);

    letzter_schritt_ =
        fahrzeugschritt(zustand_, kennwerte_, eingabe, kontakte, karosserie_, zeitschritt);
    zustand_ = letzter_schritt_.zustand;

    // Starrer Körper, semi-implizites Euler-Verfahren.
    const double dt = zeitschritt;
    const double m = kennwerte_.masse;
    const Vec3 kraft = letzter_schritt_.kraefte.kraft + Vec3{0.0, 0.0, -m * erdbeschleunigung};
    karosserie_.geschwindigkeit += kraft * (dt / m);
    karosserie_.position += karosserie_.geschwindigkeit * dt;

    const Quat q = karosserie_.ausrichtung;
    const Vec3 I = kennwerte_.traegheit;
    const Vec3 moment = drehe_zurueck(q, letzter_schritt_.kraefte.moment);
    Vec3 w_fahrzeug = drehe_zurueck(q, karosserie_.drehgeschwindigkeit);
    const Vec3& w = w_fahrzeug;
    const Vec3 kreisel = kreuz(w, {I.x * w.x, I.y * w.y, I.z * w.z});
    w_fahrzeug += Vec3{(moment.x - kreisel.x) / I.x, (moment.y - kreisel.y) / I.y,
                       (moment.z - kreisel.z) / I.z} * dt;
    karosserie_.drehgeschwindigkeit = drehe(q, w_fahrzeug);

    const Vec3 w_welt = karosserie_.drehgeschwindigkeit;
    const Quat dq = Quat{0.0, w_welt.x, w_welt.y, w_welt.z} * q;
    karosserie_.ausrichtung = normiert(
        {q.w + 0.5 * dt * dq.w, q.x + 0.5 * dt * dq.x, q.y + 0.5 * dt * dq.y, q.z + 0.5 * dt * dq.z});
}

std::uint64_t Testwelt::pruefsumme() const { return ultragravity::pruefsumme(zustand_, karosserie_); }

} // namespace ultragravity
