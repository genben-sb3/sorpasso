# Coding Standards

Gelesen beim Review. Mechanisches prüft der Hook `.githooks/pre-commit` (Sanitizer, `-Werror`, Vorhandensein von Einheitenkommentaren, `// API` an Header-Konstanten); hier stehen nur Ermessensfragen. Ergänzend gelten `GLOSSARY.md` (Namen) und `docs/adr/` (Architektur).

## Physikalische Größen

- Jede Größe mit Einheit trägt einen Einheitenkommentar an ihrer Deklaration, z. B. `double grip = 0.0;  // N`. Dimensionslose Größen bekommen `// 1` oder eine kurze Erklärung. Der Hook prüft nur, dass ein Kommentar da ist; ob er stimmt, prüft der Reviewer.
- Gleitkomma-Werte werden mit Gleitkomma-Literalen geschrieben: `0.0`, `1.0`, nicht `0`, `1`.
- Eine Formel oder Abbildung (z. B. Rad → Achse, Magic Formula, Längsschlupf, Auf-/Abbaurate) lebt an genau einer Stelle. Wird sie ein zweites Mal gebraucht, wandert sie in einen gemeinsamen Helfer. Der Reviewer sucht jede neue Formel mit `rg` nach ihrem Kernausdruck; Treffer außerhalb des Helfers sind hard. Synchron zu haltende Listen (Spaltenname/Wert) werden aus einer Tabelle erzeugt.
- Werte, die als Grenzen von `std::clamp` oder in Wurzeln/Divisionen landen, sind nachweislich im gültigen Bereich (Plausibilitätsprüfung beim Laden oder `std::max(..., 0.0)` an der Quelle).

## Schnittstelle des Physikpakets

- Öffentliche Header unter `Source/UltraGravity/Public/` zeigen nur, was Unreal später braucht. Hilfszustand, Zwischenwerte und Konstanten bleiben in `Private/`; bewusste Schnittstellenkonstanten tragen `// API`.

## Fahrzeugdateien

- Änderungen an `vehicles/*.json` sind chirurgisch: nur die betroffenen Zeilen, die bestehende Formatierung (einzeilige kleine Objekte wie `center_of_mass`) bleibt. Die Datei wird nicht über einen JSON-Serializer neu geschrieben.

## Tests

- Toleranzbänder sind eng genug, dass ein falsches Modell sie reißt. Ein Band, das auch einen deutlich zu schwachen Wert durchlässt, braucht im Test einen Kommentar, warum, und ein Folgeticket. Neben jedem nicht exakten Band steht `// measured: <value>, band: <factor>`; fehlt er, ist das hard.
