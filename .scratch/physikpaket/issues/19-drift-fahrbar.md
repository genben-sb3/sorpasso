# 19: Drift fahrbar

**What to build:** Das Sportcoupé lässt sich wie in GT6 driften. Jenseits der Haftgrenze antwortet es auf jede Aktion spürbar. Wer einen Drift nicht hält, rutscht, dreht sich oder fängt das Auto ab. Einleiten lässt sich ein Drift per Gasstoß, Lastwechsel, Handbremse oder Kupplungskick. Die Handbremse aus dem Eingabe-Paket wirkt ab hier auf die Hinterräder. Der Testfahrer bekommt einen Driftregler, der über Gegenlenken und Gas einen Driftwinkel hält und ausschließlich Eingabe-Pakete erzeugt. Alle Fahrten laufen ohne Fahrhilfen.

**Blocked by:** 04, 07

**Status:** ready-for-agent

- [ ] Jede der vier Einleitungen bringt das Auto aus einer Kurvenfahrt in einen Drift: Gasstoß, Lastwechsel, Handbremse, Kupplungskick (Kupplung bei hoher Drehzahl schnappen lassen).
- [ ] Der Driftregler hält auf trockenem Belag einen vorgegebenen Driftwinkel über mehrere Sekunden in einer Toleranz.
- [ ] Mehr Gas im Drift lässt den Driftwinkel wachsen, mehr Gegenlenken verkleinert ihn bis zum Abfangen.
- [ ] Weniger Gegenlenken oder Vollgas bei großem Driftwinkel führt zum Dreher.
- [ ] Gas weg im Drift: Das Heck greift, und das Auto pendelt zurück; aus großem Driftwinkel wirft es auf die andere Seite.
- [ ] Die Bremse im Drift ändert den Driftwinkel messbar.
- [ ] Hochschalten im Drift lässt den Driftwinkel kurz fallen, danach lässt sich der Drift weiterhalten; Runterschalten lässt den Driftwinkel wachsen.
- [ ] Gas weg in einer schnellen Kurve ohne Drift lässt das Heck kommen (Rutschen).
- [ ] Das Zurückpendeln klingt ohne weiteren Eingriff in wenigen Schwingungen ab; es entsteht kein Zittern.

## Comments

- Aus dem Grilling vom 2026-10-20: Vorbild sind die Drift-Versuche aus GT6. Eine Punktewertung gehört nicht hierher, die kommt in APEXLINE, weil sie Streckenabschnitte braucht. Begriffe: Drift, Rutschen, Dreher, Driftwinkel (siehe `GLOSSARY.md`).
- Das Sperrdifferenzial aus 07 ist für einen gehaltenen Drift entscheidend, deshalb blockiert 07.
- Pendeln (eine Antwort mit Ursache, die abklingt) ist erlaubt. Zittern (numerisches Schwingen ohne Ursache) bleibt verboten wie in 01, 03 und 06.
