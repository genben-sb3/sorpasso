# Coding Standards

Gelesen beim Review. Mechanisches prüft der Hook `.githooks/pre-commit` (Sanitizer, `-Werror`); hier stehen nur Ermessensfragen. Ergänzend gelten `GLOSSARY.md` (Namen) und `docs/adr/` (Architektur).

## Physikalische Größen

- Jede Größe mit Einheit trägt einen Einheitenkommentar an ihrer Deklaration, z. B. `double haftkraft = 0.0;  // N`. Dimensionslose Größen bekommen `// 1` oder eine kurze Erklärung.
- Gleitkomma-Werte werden mit Gleitkomma-Literalen geschrieben: `0.0`, `1.0`, nicht `0`, `1`.
- Eine Formel oder Abbildung (z. B. Rad → Achse, Magic Formula) lebt an genau einer Stelle. Wird sie ein zweites Mal gebraucht, wandert sie in einen gemeinsamen Helfer.
- Werte, die als Grenzen von `std::clamp` oder in Wurzeln/Divisionen landen, sind nachweislich im gültigen Bereich (Plausibilitätsprüfung beim Laden oder `std::max(..., 0.0)` an der Quelle).

## Schnittstelle des Physikpakets

- Öffentliche Header unter `physik/include/` zeigen nur, was Unreal später braucht. Hilfszustand und Zwischenwerte bleiben in `src/`.

## Fahrzeugdateien

- Änderungen an `fahrzeuge/*.json` sind chirurgisch: nur die betroffenen Zeilen, die bestehende Formatierung (einzeilige kleine Objekte wie `schwerpunkt`) bleibt. Die Datei wird nicht über einen JSON-Serializer neu geschrieben.

## Tests

- Toleranzbänder sind eng genug, dass ein falsches Modell sie reißt. Ein Band, das auch einen deutlich zu schwachen Wert durchlässt, braucht im Test einen Kommentar, warum, und ein Folgeticket.
