# SORPASSO (GTO)

## Build

Engine: Launcher-Build UE 5.8 unter `C:\Program Files\Epic Games\UE_5.8`. Bauen (PowerShell, im Repo-Root):

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" GTOEditor Win64 Development "-Project=$PWD\GTO.uproject" -WaitMutex
```

Der Editor muss dafür geschlossen sein (Live Coding sperrt den Build). Vorher fragen, bevor der Editor geschlossen wird.

## Tests

Unreal Automation Tests im Namensraum `GTO.` (Sim-Kern: `GTO.Sim.`), headless nach dem Build (PowerShell, im Repo-Root):

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\GTO.uproject" -ExecCmds="Automation RunTests GTO.;Quit" -unattended -nullrhi -nosplash -nosound -nopause -stdout -FullStdOutLogOutput
```

Exit-Code 0 heißt alles grün, sonst 255. Pro Test steht im Log eine Zeile `Test Completed. Result={Success|Fail}`, Fehlschläge zusätzlich als `LogAutomationController: Error:`. Für einen Teil der Tests den Filter enger fassen, z. B. `GTO.Sim.Suspension`.

Der Sim-Kern (`Source/GTOSim`) rechnet in SI-Einheiten mit Unreal-Achsen (X vorn, Y rechts, Z oben) und hängt nur von `Core` ab. Szenarien über viele Ticks laufen im Headless-Harness (`GTOHeadlessHarness.h`).

## Agent skills

### Issue tracker

Issues live in this repo's GitHub Issues, managed via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` + `docs/adr/` at the repo root, created lazily. See `docs/agents/domain.md`.
