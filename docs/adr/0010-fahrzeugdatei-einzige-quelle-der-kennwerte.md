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

## Nachtrag aus #23

- Gelesen wird die Datei vom Fahrzeug-Datenobjekt (`UGTOVehicleDataAsset::LoadVehicleParams`) über Unreals Dateizugriff; der Adapter (`UGTOVehicleSimComponent`) bekommt die geprüften Kennwerte beim Reset. Das Paket öffnet in Unreal weiterhin keine Dateien.
- Neben Meshes und Pfad hält das Datenobjekt Darstellungswerte, die zu den Meshes gehören: Lage des Karosserie-Meshes und des Lenkrads im Rahmen der Fahrzeugdatei, Reifenradius des Rad-Meshes, Größe der Kollisionsbox. Das sind keine Kennwerte: Masse und Trägheit kommen aus der Fahrzeugdatei und hängen nicht von der Box ab.
- Die Datei liegt unter `Content/VehicleFiles/` und wird über `DirectoriesToAlwaysStageAsUFS` in `Config/DefaultGame.ini` gepackt.
