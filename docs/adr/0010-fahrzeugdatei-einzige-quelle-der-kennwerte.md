---
status: accepted
---

# Die JSON-Fahrzeugdatei ist die einzige Quelle der Kennwerte

Alle Kennwerte eines Autos (Masse, Geometrie, Fahrwerk, Reifen, Antrieb, Aero) stehen nur in seiner JSON-Fahrzeugdatei. Das Fahrzeug-Datenobjekt in Unreal (`UGTOVehicleDataAsset`) hält nur die Meshes und den Verweis auf diese Datei, keine Kennwerte. Naheliegend wäre das Umgekehrte gewesen: Kennwerte als UPROPERTYs im Datenobjekt, im Editor sichtbar und einstellbar. Das haben wir verworfen, weil die Tests und das Abstimm-Werkzeug des Physikpakets ohne Unreal laufen (ADR 0003) und dieselbe Datei lesen müssen; zwei Quellen hätten jede Abstimmung an zwei Stellen verlangt und auseinanderlaufen lassen.

## Konsequenzen

- Die Fahrzeugdatei liegt im Content-Ordner als Rohdatei (kein Unreal-Asset) und kommt beim Packen mit, damit das Spiel auch außerhalb des Editors fährt.
- Der Adapter liest die Datei über Unreals Dateizugriff und übergibt den Text dem Physikpaket, das ihn parst und prüft; das Paket öffnet in Unreal selbst keine Dateien.
- Ein Fehler in der Fahrzeugdatei wird im Spiel mit seinem Feldpfad gemeldet, so wie in den Ladetests.
- Kennwerte ändert man in der JSON-Datei, nicht im Editor. Änderungen sind chirurgisch (siehe `CODING_STANDARDS.md`).
