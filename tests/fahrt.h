#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <numbers>
#include <optional>

#include "doctest/doctest.h"
#include "ultragravity/testwelt.h"

namespace ultragravity::test {

inline constexpr double hundert_kmh = 100.0 / 3.6; // m/s
inline constexpr double u_min = 2.0 * std::numbers::pi / 60.0; // rad/s je U/min


inline double drehzahl(const Testwelt& welt) { return welt.letzter_schritt().animationsdaten.drehzahl; }

inline Kennwerte sportcoupe() {
    LadeErgebnis ergebnis = lade_fahrzeugdatei(UG_FAHRZEUGE_DIR "/sportcoupe.json");
    REQUIRE_FALSE(ergebnis.fehler.has_value());
    return ergebnis.kennwerte;
}

inline Kennwerte mit_automatik() {
    Kennwerte auto_ = sportcoupe();
    auto_.antrieb.getriebe.modus = Getriebemodus::automatik;
    auto_.antrieb.getriebe.automatik = {.hochschaltdrehzahl = 6500.0 * u_min,
                                        .runterschaltdrehzahl = 2500.0 * u_min,
                                        .bremsschaltdrehzahl = 4000.0 * u_min,
                                        .haltezeit = 0.8,
                                        .einkuppelbereich = 1500.0 * u_min,
                                        .einkuppelzeit = 0.15,
                                        .driftwinkel = 10.0 * std::numbers::pi / 180.0};
    return auto_;
}

inline int gang(const Testwelt& welt) { return welt.letzter_schritt().zustand.gang; }

inline EingabePaket fahren(double gas, double bremse = 0.0, double lenkung = 0.0) {
    EingabePaket p;
    p.gangwunsch = 1; // in der Automatik: Fahren
    p.gas = gas;
    p.bremse = bremse;
    p.lenkung = lenkung;
    return p;
}

// Liefert für jeden Tick der Fahrt ein Eingabe-Paket mit laufender Tick-Nummer.
class Fahrt {
public:
    explicit Fahrt(Testwelt& welt) : welt_(welt) {}

    void fahre(double sekunden, EingabePaket paket = {}) {
        for (int i = 0; i < ticks(sekunden); ++i) einen_tick(paket);
    }

    void einen_tick(EingabePaket paket) {
        paket.tick = tick_++;
        welt_.tick(paket);
    }

    static int ticks(double sekunden) { return static_cast<int>(std::lround(sekunden / Testwelt::zeitschritt)); }

private:
    Testwelt& welt_;
    std::uint32_t tick_ = 0;
};

inline double laengsgeschwindigkeit(const Testwelt& welt) {
    const Karosseriebewegung& k = welt.karosserie();
    return skalar(k.geschwindigkeit, drehe(k.ausrichtung, {1.0, 0.0, 0.0}));
}

// Erkennt Schwingen an einer Größe: Wechselt ihre Änderung von Tick zu Tick
// dreimal hintereinander das Vorzeichen, zappelt sie, statt einem Verlauf zu folgen.
// Änderungen bis zum Rauschen zählen nicht.
class Schwingungsmelder {
public:
    explicit Schwingungsmelder(double rauschen = 0.01) : rauschen_(rauschen) {}

    void nimm(double wert) {
        if (bisher_) {
            const double aenderung = wert - *bisher_;
            if (std::abs(aenderung) > rauschen_) {
                const int richtung = aenderung > 0.0 ? 1 : -1;
                wechsel_in_folge_ = richtung == -richtung_ ? wechsel_in_folge_ + 1 : 0;
                richtung_ = richtung;
                laengste_folge_ = std::max(laengste_folge_, wechsel_in_folge_);
            }
        }
        bisher_ = wert;
    }

    int laengste_folge() const { return laengste_folge_; }

private:
    double rauschen_; // Einheit der Größe je Tick
    std::optional<double> bisher_;
    int richtung_ = 0;
    int wechsel_in_folge_ = 0;
    int laengste_folge_ = 0;
};

// Fährt an und schaltet kurz vor dem Begrenzer hoch: Kupplung treten, Gas weg,
// nächster Gang, nach der Schaltzeit die Kupplung zügig kommen lassen. In den
// unteren Gängen gibt er nur so viel Gas, dass die Hinterräder kaum durchdrehen.
class Testfahrer {
public:
    static constexpr double einlegen = 0.3;         // s Kupplung getreten vor dem Anfahren
    static constexpr double kupplung_anfahren = 0.5; // s zum Kommenlassen beim Anfahren
    static constexpr double kupplung_schalten = 0.15; // s zum Kommenlassen nach dem Schalten
    static constexpr double schaltdrehzahl = 6800.0 * u_min;
    static constexpr std::array<double, 3> gas_untere_gaenge{0.75, 0.95, 1.0}; // 1

    explicit Testfahrer(const Kennwerte& k)
        : gaenge_(static_cast<int>(k.antrieb.getriebe.uebersetzungen.size())),
          schaltpause_(k.antrieb.getriebe.schaltzeit + 0.05) {}

    EingabePaket paket(const Testwelt& welt) {
        EingabePaket p;
        p.gangwunsch = static_cast<std::int8_t>(gang_);
        if (zeit_ < einlegen) {
            p.kupplung = 1.0;
        } else if (zeit_ < einlegen + kupplung_anfahren) {
            p.gas = gas();
            p.kupplung = 1.0 - (zeit_ - einlegen) / kupplung_anfahren;
        } else {
            const double seit_schalten = zeit_ - geschaltet_;
            if (gang_ < gaenge_ && seit_schalten > schaltpause_ + kupplung_schalten &&
                drehzahl(welt) > schaltdrehzahl) {
                ++gang_;
                geschaltet_ = zeit_;
                p.gangwunsch = static_cast<std::int8_t>(gang_);
                p.kupplung = 1.0;
            } else if (seit_schalten < schaltpause_) {
                p.kupplung = 1.0;
            } else {
                p.gas = gas();
                p.kupplung = std::max(1.0 - (seit_schalten - schaltpause_) / kupplung_schalten, 0.0);
            }
        }
        zeit_ += Testwelt::zeitschritt;
        return p;
    }

    int gang() const { return gang_; }
    bool eingekuppelt() const { return zeit_ - geschaltet_ > schaltpause_ + kupplung_schalten; }

private:
    double gas() const { return gang_ <= 3 ? gas_untere_gaenge[gang_ - 1] : 1.0; }

    int gaenge_;
    double schaltpause_; // s
    int gang_ = 1;
    double zeit_ = 0.0;        // s seit Beginn des Anfahrens
    double geschaltet_ = -1e9; // s, Zeitpunkt des letzten Gangwechsels
};

} // namespace ultragravity::test
