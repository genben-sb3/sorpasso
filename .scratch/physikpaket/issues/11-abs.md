# 11: ABS

**What to build:** ABS als Fahrhilfe, an- und abschaltbar über das Eingabe-Paket. Es greift nur zwischen Eingabe-Paket und Bremsen ein (prädiktiv, regelt auf den Peak-Schlupf, aktiv erst oberhalb einer Mindestgeschwindigkeit). Das Abstimm-Werkzeug bekommt das Testmanöver „Bremsweg 100–0 km/h mit ABS“.

**Blocked by:** 09

**Status:** ready-for-agent

- [ ] Mit ABS blockiert bei Vollbremsung kein Rad oberhalb der Mindestgeschwindigkeit.
- [ ] Mit ABS ist das Auto bei Vollbremsung lenkbar (Lenkeinschlag ändert die Richtung), ohne ABS nicht.
- [ ] Der Bremsweg mit ABS ist auf trockenem Belag nicht länger als ohne (Toleranz).
- [ ] Die Tabelle zeigt beide Bremsweg-Testmanöver.
