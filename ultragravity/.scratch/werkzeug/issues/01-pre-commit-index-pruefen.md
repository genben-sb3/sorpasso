# 01: Pre-Commit-Hook prüft den Index statt des Arbeitsbaums

**What to build:** `.githooks/pre-commit` baut und testet heute den Arbeitsbaum. Bei einem Teil-Commit (`git add -p`) kann der Hook „grün“ melden, obwohl der committete Stand nicht baut oder Tests verliert. Der Prüflauf soll genau den Inhalt des Index prüfen.

**Blocked by:** –

**Status:** needs-triage

- [ ] Ein Commit, dessen Index nicht baut, wird abgelehnt, auch wenn der Arbeitsbaum baut.
- [ ] Nicht gestagte Änderungen und ungetrackte Dateien sind nach dem Hook unverändert da, auch bei Fehlschlag und bei Ctrl-C.
- [ ] Der inkrementelle Build in `build-pruefen` bleibt nutzbar; der Hook erzwingt keinen Vollbau bei jedem Commit.

## Comments

- Aus dem Review von `3b708a6`. Mögliche Wege: `git stash push --keep-index --include-untracked` mit Wiederherstellen per `trap` (Risiko: Stash-Konflikte, verlorene Änderungen bei Abbruch), oder den Index per `git checkout-index` bzw. `git worktree` in ein eigenes Verzeichnis legen und dort bauen (Risiko: eigenes Build-Verzeichnis, langsamer erster Lauf). Vor der Umsetzung entscheiden.
