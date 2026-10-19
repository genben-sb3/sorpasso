# Räder und Antriebsstrang explizit mit Substeps, Constraints erst bei Instabilität

Räder, Bremsen, Kupplung und Antriebsstrang bilden ein steifes System. Statt gleich einen Constraint-Löser wie VDrift zu bauen, rechnen wir explizit mit 8 Substeps pro Testwelt-Tick (rund 960 Hz) und setzen Bremsen und Kupplung als begrenzte Impulse um, weil das einfacher zu verstehen und zu testen ist. Bleibt das instabil, wechseln wir auf Constraints (vgl. `GTO/research/fahrphysik.md`, Abschnitte 3 und 9.4).
