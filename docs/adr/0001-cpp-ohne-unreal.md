# Physikpaket in C++20 ohne jede Unreal-Abhängigkeit

Das Physikpaket soll später unverändert in APEXLINE (Unreal Engine 5, C++) einziehen, wird aber unter Linux im Terminal gebaut und getestet. Deshalb ist es reines C++20 mit nur der Standardbibliothek: keine Unreal-Header, kein UObject, keine Engine-Mathematik. Gebaut wird mit CMake, getestet mit doctest. Unreal bindet den Code später über einen dünnen Adapter ein (vgl. `GTO/research/fahrphysik.md`, Abschnitt 9). Eine TypeScript-Lernversion haben wir verworfen, weil das Ziel echte Wiederverwendung ist.
