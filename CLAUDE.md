# SORPASSO (GTO)

## Build

Engine: Launcher-Build UE 5.8 unter `C:\Program Files\Epic Games\UE_5.8`. Bauen (PowerShell, im Repo-Root):

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" GTOEditor Win64 Development "-Project=$PWD\GTO.uproject" -WaitMutex
```

Der Editor muss dafür geschlossen sein (Live Coding sperrt den Build). Vorher fragen, bevor der Editor geschlossen wird.

## Agent skills

### Issue tracker

Issues live in this repo's GitHub Issues, managed via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` + `docs/adr/` at the repo root, created lazily. See `docs/agents/domain.md`.
