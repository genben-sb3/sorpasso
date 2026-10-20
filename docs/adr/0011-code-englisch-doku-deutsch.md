---
status: accepted
---

# Code und Daten Englisch, Doku Deutsch

Code ist Englisch: Bezeichner, Kommentare, Testnamen, Dateinamen, die Feldnamen der Fahrzeugdatei und Fehlermeldungen. Doku ist Deutsch: ADRs, Glossar (`CONTEXT.md`), Coding Standards, `CLAUDE.md`, README, Issues und Commit-Nachrichten. Das Physikpaket war durchgehend Deutsch, SORPASSO hatte schon englischen Code und deutsche Doku; wir haben das Paket an SORPASSO angepasst statt umgekehrt, weil Unreal-Code und die Quellen der Fahrzeugdynamik englische Namen verwenden und die Menschen im Projekt auf Deutsch planen und schreiben.

## Konsequenzen

- Das Glossar führt den englischen Begriff aus dem Code als kanonischen Namen und das deutsche Wort als Synonym, damit man auf Deutsch reden und den Code trotzdem finden kann.
- Die Feldnamen der Fahrzeugdatei wechselten hart auf Englisch; alte deutsche Namen werden beim Laden nicht unterstützt.
- Das Physikpaket bleibt Standard-C++ mit `snake_case` ohne Unreal-Präfixe im Namensraum `ultragravity`; der Unreal-Code in `Source/GTO` folgt den Unreal-Konventionen.
