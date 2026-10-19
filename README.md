# ultragravity

Fahrphysik für APEXLINE, gebaut und getestet im Terminal. Begriffe stehen in `GLOSSARY.md`, Entscheidungen in `docs/adr/`.

## Bauen und testen

```sh
cmake --workflow --preset standard   # konfigurieren, bauen, alle Tests
```

Danach reichen `cmake --build build` und `ctest --test-dir build`.
Einzelne Tests: `ctest --test-dir build -R <Teil des TEST_CASE-Namens>`.

Vor jedem Commit läuft `cmake --workflow --preset pruefen` (Sanitizer, libstdc++-Assertions, `-Werror`, Build in `build-pruefen/`). Den Hook einmal pro Klon aktivieren:

```sh
git config core.hooksPath .githooks
```

Nur das Physikpaket bauen (das Ziel, das nach APEXLINE umzieht):

```sh
cmake --build build --target ultragravity_physik
```

## Aufbau

- `physik/`: das Physikpaket, nur Standardbibliothek, ohne Exceptions und RTTI
- `testwelt/`: Ersatz für Unreal: starrer Körper, Testgelände, Radkontakte
- `fahrzeuge/`: Fahrzeugdateien
- `tests/`: doctest-Tests an den Nähten aus der Spec
- `third_party/`: doctest 2.4.11 (Header plus `cmake/` für `doctest_discover_tests`) und nlohmann/json 3.11.3 als einzelne Header-Datei
