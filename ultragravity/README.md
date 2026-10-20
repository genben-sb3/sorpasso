# ultragravity

CMake-Projekt des Physikpakets von SORPASSO: baut die Quellen des Unreal-Moduls `../Source/UltraGravity/` ohne Unreal und testet sie im Terminal. Begriffe stehen in `../CONTEXT.md`, Entscheidungen in `../docs/adr/`, Regeln fürs Review in `../CODING_STANDARDS.md`.

## Bauen und testen

```sh
cmake --workflow --preset standard   # konfigurieren, bauen, alle Tests
```

Danach reichen `cmake --build build` und `ctest --test-dir build`.
Einzelne Tests: `ctest --test-dir build -R <Teil des TEST_CASE-Namens>`.

Vor jedem Commit mit Physik-Dateien läuft `cmake --workflow --preset pruefen` (Sanitizer, libstdc++-Assertions, `-Werror`, Build in `build-pruefen/`). Den Hook einmal pro Klon aktivieren (aus dem Repo-Root; `core.hooksPath` bleibt unverändert, damit die Git-LFS-Hooks weiterlaufen):

```sh
ln -s ../../.githooks/pre-commit "$(git rev-parse --git-common-dir)/hooks/pre-commit"
```

Nur das Physikpaket bauen (ohne Testwelt und Tests):

```sh
cmake --build build --target ultragravity_physics
```

## Aufbau

- `physics/`: CMake-Ziel des Physikpakets (Quellen im Unreal-Modul `../Source/UltraGravity/`), nur Standardbibliothek, ohne Exceptions und RTTI
- `test_world/`: Ersatz für Unreal: starrer Körper, Testgelände, Radkontakte
- `vehicles/`: Fahrzeugdateien
- `tests/`: doctest-Tests an den Nähten aus der Spec
- `third_party/`: doctest 2.4.11 (Header plus `cmake/` für `doctest_discover_tests`); nlohmann/json 3.11.3 liegt im Modul unter `../Source/UltraGravity/ThirdParty/`
