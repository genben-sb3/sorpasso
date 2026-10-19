# ultragravity

Fahrphysik für APEXLINE, gebaut und getestet im Terminal. Begriffe stehen in `GLOSSARY.md`, Entscheidungen in `docs/adr/`.

## Bauen und testen

```sh
cmake --workflow --preset standard   # konfigurieren, bauen, alle Tests
```

Danach reichen `cmake --build build` und `ctest --test-dir build`.

Nur das Physikpaket bauen (das Ziel, das nach APEXLINE umzieht):

```sh
cmake --build build --target ultragravity_physik
```

## Aufbau

- `physik/`: das Physikpaket, nur Standardbibliothek, ohne Exceptions und RTTI
- `testwelt/`: Ersatz für Unreal: starrer Körper, Testgelände, Radkontakte
- `fahrzeuge/`: Fahrzeugdateien
- `tests/`: doctest-Tests an den Nähten aus der Spec
- `third_party/`: doctest 2.4.11 und nlohmann/json 3.11.3, je als einzelne Header-Datei
