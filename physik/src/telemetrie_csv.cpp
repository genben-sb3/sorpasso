#include "ultragravity/telemetrie_csv.h"

#include <array>
#include <charconv>
#include <ostream>
#include <string>
#include <string_view>

namespace ultragravity {
namespace {

constexpr std::array<std::string_view, anzahl_raeder> radkuerzel{"vl", "vr", "hl", "hr"};

// Schreibt Felder durch Kommas getrennt und beendet die Zeile.
class Zeile {
public:
    explicit Zeile(std::ostream& aus) : aus_(aus) {}
    ~Zeile() { aus_ << '\n'; }

    void text(std::string_view t) {
        trenne();
        aus_ << t;
    }
    void zahl(double wert) {
        trenne();
        std::array<char, 32> puffer;
        const auto ergebnis = std::to_chars(puffer.data(), puffer.data() + puffer.size(), wert);
        aus_.write(puffer.data(), ergebnis.ptr - puffer.data());
    }
    void ganzzahl(long long wert) {
        trenne();
        aus_ << wert;
    }

private:
    void trenne() {
        if (!erstes_) aus_ << ',';
        erstes_ = false;
    }

    std::ostream& aus_;
    bool erstes_ = true;
};

void kopf_je_rad(Zeile& z, const std::string& name, const std::string& einheit) {
    for (std::string_view rad : radkuerzel) z.text(name + "_" + std::string(rad) + "[" + einheit + "]");
}

void werte_je_rad(Zeile& z, const std::array<double, anzahl_raeder>& werte) {
    for (double w : werte) z.zahl(w);
}

} // namespace

void schreibe_telemetrie_kopfzeile(std::ostream& aus) {
    Zeile z(aus);
    z.text("tick[1]");
    z.text("geschwindigkeit[m/s]");
    z.text("motordrehzahl[rad/s]");
    z.text("gang[1]");
    z.text("lenkung[1]");
    z.text("gas[1]");
    z.text("bremse[1]");
    z.text("kupplung[1]");
    z.text("handbremse[1]");
    kopf_je_rad(z, "schlupf", "1");
    kopf_je_rad(z, "schraeglauf", "rad");
    kopf_je_rad(z, "radlast", "N");
    kopf_je_rad(z, "kraft_laengs", "N");
    kopf_je_rad(z, "kraft_quer", "N");
}

void schreibe_telemetrie_zeile(std::ostream& aus, const EingabePaket& eingabe, const SchrittErgebnis& ergebnis) {
    Zeile z(aus);
    z.ganzzahl(eingabe.tick);
    z.zahl(ergebnis.telemetrie.geschwindigkeit);
    z.zahl(ergebnis.zustand.motordrehzahl);
    z.ganzzahl(ergebnis.zustand.gang);
    z.zahl(eingabe.lenkung);
    z.zahl(eingabe.gas);
    z.zahl(eingabe.bremse);
    z.zahl(eingabe.kupplung);
    z.zahl(eingabe.handbremse);
    werte_je_rad(z, ergebnis.telemetrie.schlupf);
    werte_je_rad(z, ergebnis.telemetrie.schraeglauf);
    werte_je_rad(z, ergebnis.telemetrie.radlast);
    werte_je_rad(z, ergebnis.telemetrie.kraft_laengs);
    werte_je_rad(z, ergebnis.telemetrie.kraft_quer);
}

} // namespace ultragravity
