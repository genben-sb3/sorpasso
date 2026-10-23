# Coding Standards

Gelten für das Physikpaket: das Unreal-Modul `Source/UltraGravity/`, das CMake-Projekt `ultragravity/` (Testwelt, Tests) und die Fahrzeugdateien in `Content/VehicleFiles/`. Gelesen beim Review. Mechanisches prüft der Hook `.githooks/pre-commit` mit `.githooks/check-units.sh` (Sanitizer, `-Werror`, Vorhandensein von Einheitenkommentaren, `// API` an Header-Konstanten, `// measured:` an Bändern, Umrechnungen und Kernformeln nur im Helfer, Gleitkomma-Literale); hier stehen nur Ermessensfragen. Ergänzend gelten `CONTEXT.md` (Namen) und `docs/adr/` (Architektur).

## Physikalische Größen

- Jede Größe mit Einheit trägt einen Einheitenkommentar an ihrer Deklaration, z. B. `double grip = 0.0;  // N`. Dimensionslose Größen bekommen `// 1` oder eine kurze Erklärung. Der Hook prüft nur, dass ein Kommentar da ist; ob er stimmt, prüft der Reviewer.
- Gleitkomma-Werte werden mit Gleitkomma-Literalen geschrieben: `0.0`, `1.0`, nicht `0`, `1`. Der Hook prüft das für Deklarationen mit Literal.
- Eine Formel oder Abbildung (z. B. Rad → Achse, Magic Formula, Längsschlupf, Auf-/Abbaurate) lebt an genau einer Stelle. Wird sie ein zweites Mal gebraucht, wandert sie in einen gemeinsamen Helfer (Liste in `docs/agents/helpers.md`). Umrechnungen, Driftwinkel und g prüft der Hook; für alle anderen Formeln sucht der Reviewer jede neue Formel mit `rg` nach ihrem Kernausdruck; Treffer außerhalb des Helfers sind hard. Synchron zu haltende Listen (Spaltenname/Wert) werden aus einer Tabelle erzeugt.
- Jede Zahl in einer Bedingung ist eine benannte Konstante mit Einheitenkommentar, auch eine einmalig verwendete Grenze. Eine neue Konstante steht nie zwischen einem Kommentarblock und der Konstantengruppe, die er beschreibt. Lokale Variablen heißen nach dem Fall (`without_brakes`), nicht `a`/`b`.
- Werte, die als Grenzen von `std::clamp` oder in Wurzeln/Divisionen landen, sind nachweislich im gültigen Bereich (Plausibilitätsprüfung beim Laden oder `std::max(..., 0.0)` an der Quelle).

## Schnittstelle des Physikpakets

- Öffentliche Header unter `Source/UltraGravity/Public/` zeigen nur, was der Unreal-Adapter braucht. Hilfszustand, Zwischenwerte und Konstanten bleiben in `Private/`; bewusste Schnittstellenkonstanten tragen `// API`.

## Fahrzeugdateien

- Änderungen an Fahrzeugdateien (`Content/VehicleFiles/*.json`) sind chirurgisch: nur die betroffenen Zeilen, die bestehende Formatierung (einzeilige kleine Objekte wie `center_of_mass`) bleibt. Die Datei wird nicht über einen JSON-Serializer neu geschrieben.

## Tests

- Toleranzbänder sind eng genug, dass ein falsches Modell sie reißt. Ein Band, das auch einen deutlich zu schwachen Wert durchlässt, braucht im Test einen Kommentar, warum, und ein Folgeticket. Neben jedem nicht exakten Band steht `// measured: <value>, band: <factor>`; der Hook prüft das Vorhandensein, der Reviewer den Wert. Bricht die Absicht eines Tests mit einer gewollten Änderung der Physik, wird das Band nicht gelockert: Der Test bleibt, wie er ist, wird mit `doctest::should_fail()` markiert und bekommt einen Kommentar mit dem Messwert und einem Folgeticket.
