# SORPASSO (GTO)

## Build

Engine: Launcher-Build UE 5.8 unter `C:\Program Files\Epic Games\UE_5.8`. Bauen (PowerShell, im Repo-Root):

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" GTOEditor Win64 Development "-Project=$PWD\GTO.uproject" -WaitMutex
```

Der Editor muss dafür geschlossen sein (Live Coding sperrt den Build). Vorher fragen, bevor der Editor geschlossen wird.

## Tests

Unreal Automation Tests im Namensraum `GTO.`, headless nach dem Build (PowerShell, im Repo-Root):

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\GTO.uproject" -ExecCmds="Automation RunTests GTO.;Quit" -unattended -nullrhi -nosplash -nosound -nopause -stdout -FullStdOutLogOutput
```

Exit-Code 0 heißt alles grün, sonst 255. Pro Test steht im Log eine Zeile `Test Completed. Result={Success|Fail}`, Fehlschläge zusätzlich als `LogAutomationController: Error:`. Für einen Teil der Tests den Filter enger fassen, z. B. `GTO.Smoke.Track`.

`GTO.Smoke.` spielt die Testmaps `/Game/Maps/FlatTest` und `/Game/Maps/TestTrack` per PIE und prüft nur das Zusammenspiel von Adapter und Chaos; die Fahrphysik selbst testet das CMake-Projekt (ADR 0009). Zum Vergleich ist die Testwelt des Physikpakets in die Tests einkompiliert (`Source/GTO/Tests/GTOTestWorld.cpp`). Weitere Konsolenbefehle vor dem Testlauf mit Komma trennen (`-ExecCmds="t.MaxFPS 30,Automation RunTests GTO.;Quit"`); das Semikolon gehört zum Automation-Befehl.

Der Adapter (`UGTOVehicleSimComponent`) ist die einzige Stelle, an der zwischen Unreal (cm, Y rechts) und dem Physikpaket (SI, ISO 8855, Y links) umgerechnet wird. Was er nach außen gibt, ist in Metern mit Unreal-Achsen, wie Strecke und Rundenzeit.

## Physikpaket

Die Fahrphysik liegt im Unreal-Modul `Source/UltraGravity` (C++20, nur Standardbibliothek, SI-Einheiten mit ISO-8855-Achsen: X vorn, Y links, Z oben). Das CMake-Projekt `ultragravity/` baut dieselben Quellen ohne Unreal, dazu Testwelt und doctest-Tests; dafür braucht es keine Unreal-Installation. Bauen und alle Tests (im Repo-Root):

```
cd ultragravity && cmake --workflow --preset check
```

`check` baut mit Sanitizern und `-Werror` in `ultragravity/build-check/`; schneller ohne Sanitizer ist `--preset standard` (Build in `ultragravity/build/`, einzelne Tests mit `ctest --test-dir build -R <Name>`). Neue `.cpp` im Modul in `ultragravity/physics/CMakeLists.txt` eintragen (UBT findet sie von selbst, CMake nicht), neue Testdateien in `ultragravity/tests/CMakeLists.txt`. Neue Funktionen in öffentlichen Headern tragen `UG_API` (`ultragravity/export.h`), sonst findet der Linker sie in Unreal-Editor-Builds nicht.

Unter Windows braucht CMake eine Developer-Shell von VS 2022 (`vcvars64.bat`) mit dessen Clang (`CC=clang`, `CXX=clang++`, Ninja auf dem PATH). `standard` läuft dort; `check` linkt nicht, weil VS 2022 die x64-ASan-Bibliothek der STL nicht mitbringt. Ersatz bis dahin: `check` in einem eigenen Build-Ordner mit `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` und den Flags ohne `address` und `-D_GLIBCXX_ASSERTIONS` konfigurieren (UBSan und `-Werror` bleiben).

Die Fahrzeugdateien liegen als JSON unter `Content/VehicleFiles/` (ADR 0010): einzige Quelle der Kennwerte, gelesen vom Fahrzeug-Datenobjekt (`UGTOVehicleDataAsset`) über Unreals Dateizugriff, beim Packen mitgenommen (`DirectoriesToAlwaysStageAsUFS` in `Config/DefaultGame.ini`). Die CMake-Tests lesen dieselben Dateien.

Der Pre-Commit-Hook `.githooks/pre-commit` prüft Einheitenkommentare und läuft `check`, aber nur, wenn der Commit Dateien unter `Source/UltraGravity`, `ultragravity` oder `Content/VehicleFiles` enthält. Einmal pro Klon aktivieren (im Repo-Root):

```
ln -s ../../.githooks/pre-commit "$(git rev-parse --git-common-dir)/hooks/pre-commit"
```

`core.hooksPath` bleibt unverändert, damit die Git-LFS-Hooks in `.git/hooks` weiterlaufen.

Für Modul und CMake-Projekt gelten die Coding Standards in `CODING_STANDARDS.md`. Vor jeder neuen Formel `docs/agents/helpers.md` lesen und mit `rg` suchen. Code, Dateinamen und Feldnamen der Fahrzeugdatei sind Englisch; Doku, Issues und Commit-Nachrichten Deutsch (ADR 0011).

## Binäre Assets

Testmaps und `DA_SportsCar` erzeugt `Scripts/create_content.py` (Aufruf steht im Skript). Nach Änderungen am Skript neu ausführen statt die Assets von Hand zu bearbeiten. Fremde Assets kommen mit Quelle und Lizenz in `CREDITS.md`.

## Agent skills

### Issue tracker

Issues live in this repo's GitHub Issues, managed via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` + `docs/adr/` at the repo root, created lazily. See `docs/agents/domain.md`.
