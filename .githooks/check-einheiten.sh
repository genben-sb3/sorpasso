#!/bin/sh
# Prüft nur neu hinzugefügte Zeilen im Modul Source/UltraGravity/ ohne ThirdParty/ (git diff --cached):
#  1. double/float-Deklarationen tragen einen //-Kommentar (Einheit oder "1").
#  2. constexpr-Variablen in Public/ tragen "// API" (bewusste Schnittstelle), sonst gehören sie nach Private/.
git diff --cached -U0 --diff-filter=AM -- 'Source/UltraGravity/*.h' 'Source/UltraGravity/*.cpp' ':!Source/UltraGravity/ThirdParty' | awk '
/^\+\+\+ b\// { file = substr($0, 7); next }
/^@@/ { split($3, p, ","); n = substr(p[1], 2) + 0; next }
/^\+/ {
    line = substr($0, 2)
    if (line ~ /^[ \t]*(const |constexpr |inline constexpr |static constexpr )?(double|float) [A-Za-z_0-9]+ ?(=|;|\{)/ && line !~ /\/\//) {
        printf "%s:%d: Einheitenkommentar fehlt (// N, // rad/s, ... oder // 1)\n", file, n; bad = 1
    }
    if (file ~ /^Source\/UltraGravity\/Public\// && line ~ /^[ \t]*(inline )?constexpr [A-Za-z_:0-9]+ [A-Za-z_0-9]+ ?(=|\{)/ && line !~ /\/\/ API/) {
        printf "%s:%d: Konstante im öffentlichen Header: nach Private/ verschieben oder mit // API markieren\n", file, n; bad = 1
    }
    n++
}
END { exit bad }
'
