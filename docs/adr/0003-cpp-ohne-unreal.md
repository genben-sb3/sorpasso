# Physikpaket in C++20 ohne jede Unreal-Abhängigkeit

Das Physikpaket läuft in SORPASSO (Unreal Engine 5, C++), wird aber unter Linux im Terminal gebaut und getestet. Deshalb ist es reines C++20 mit nur der Standardbibliothek: keine Unreal-Header, kein UObject, keine Engine-Mathematik. Gebaut wird mit CMake, getestet mit doctest. Unreal bindet den Code über einen dünnen Adapter ein (vgl. `research/fahrphysik.md`, Abschnitt 9). Eine TypeScript-Lernversion haben wir verworfen, weil das Ziel echte Wiederverwendung ist.

In SORPASSO sind die Quellen das Unreal-Modul `Source/UltraGravity`; nur seine Moduldatei (`UltraGravityModule.cpp`) bindet einen Unreal-Header ein. Das CMake-Projekt unter `ultragravity/` baut dieselben Quellen ohne diese Datei.
