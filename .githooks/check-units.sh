#!/bin/sh
# Checks only newly added lines in the module Source/UltraGravity/ without ThirdParty/ (git diff --cached):
#  1. double/float declarations carry a // comment (unit or "1").
#  2. constexpr variables in Public/ carry "// API" (deliberate interface), otherwise they belong in Private/.
git diff --cached --no-renames -U0 --diff-filter=AM -- 'Source/UltraGravity/*.h' 'Source/UltraGravity/*.cpp' ':!Source/UltraGravity/ThirdParty' | awk '
/^\+\+\+ b\// { file = substr($0, 7); next }
/^@@/ { split($3, p, ","); n = substr(p[1], 2) + 0; next }
/^\+/ {
    line = substr($0, 2)
    if (line ~ /^[ \t]*(const |constexpr |inline constexpr |static constexpr )?(double|float) [A-Za-z_0-9]+ ?(=|;|\{)/ && line !~ /\/\//) {
        printf "%s:%d: unit comment missing (// N, // rad/s, ... or // 1)\n", file, n; bad = 1
    }
    if (file ~ /^Source\/UltraGravity\/Public\// && line ~ /^[ \t]*(inline )?constexpr [A-Za-z_:0-9]+ [A-Za-z_0-9]+ ?(=|\{)/ && line !~ /\/\/ API/) {
        printf "%s:%d: constant in a public header: move it to Private/ or mark it with // API\n", file, n; bad = 1
    }
    n++
}
END { exit bad }
'
