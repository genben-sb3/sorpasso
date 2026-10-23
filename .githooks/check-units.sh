#!/bin/sh
# Checks only newly added lines of the commit (git diff --cached). UG_CHECK_RANGE, e.g. "main...HEAD",
# checks the lines a range of commits adds instead.
# In the module Source/UltraGravity/ without ThirdParty/:
#  1. double/float declarations carry a // comment (unit or "1").
#  2. constexpr variables in Public/ carry "// API" (deliberate interface), otherwise they belong in Private/.
#  7. double/float declarations with a literal write it as floating point (15000.0, not 15000).
# In the physics package, the test world, the tuning tool and the tests:
#  3. Tolerance bands in ultragravity/tests/ carry "measured" (presence; the reviewer checks the value).
#  4. Raw conversion factors 3.6 and pi/60 only in units.h; elsewhere kmh / rpm from ultragravity/units.h.
#  5. The drift angle formula (atan2 with .y) only in vector_math.h, 9.81 only in units.h; exception:
#     "// helper-ok: <reason>" on the same line (docs/agents/helpers.md).
#  6. Integer struct fields (int, std::optional<int|double>, std::size_t) in headers carry a // comment.
range=${UG_CHECK_RANGE:---cached}
added() { git diff $range --no-renames -U0 --diff-filter=AM -- "$@"; }

# Prefix awk program: tracks file and line number of every added line.
track='
/^\+\+\+ b\// { file = substr($0, 7); next }
/^@@/ { split($3, p, ","); n = substr(p[1], 2) + 0; next }
'

# 1, 2 and 7.
added 'Source/UltraGravity/*.h' 'Source/UltraGravity/*.cpp' ':!Source/UltraGravity/ThirdParty' | awk "$track"'
/^\+/ {
    line = substr($0, 2)
    if (line ~ /^[ \t]*(const |constexpr |inline constexpr |static constexpr )?(double|float) [A-Za-z_0-9]+ ?(=|;|\{)/ && line !~ /\/\//) {
        printf "%s:%d: unit comment missing (// N, // rad/s, ... or // 1)\n", file, n; bad = 1
    }
    if (file ~ /^Source\/UltraGravity\/Public\// && line ~ /^[ \t]*(inline )?constexpr [A-Za-z_:0-9]+ [A-Za-z_0-9]+ ?(=[^=]|\{)/ && line !~ /operator/ && line !~ /\/\/ API/) {
        printf "%s:%d: constant in a public header: move it to Private/ or mark it with // API\n", file, n; bad = 1
    }
    if (line ~ /(double|float) [A-Za-z_0-9]+ ?(=|\{) *-?[0-9]+ *[;,}]/) {
        printf "%s:%d: write the floating point literal with a point (15000.0)\n", file, n; bad = 1
    }
    n++
}
END { exit bad }
' || bad=1

# 3. A band (epsilon/margin, or CHECK/REQUIRE with a comparison and a floating point literal) carries
# "measured" on the same, the previous or the next line.
added 'ultragravity/tests/*.cpp' | awk "$track"'
/^\+/ {
    line = substr($0, 2)
    if (line ~ /measured/) seen[file, n] = 1
    if (line ~ /(epsilon|margin)\(/ || (line ~ /(CHECK|REQUIRE)/ && line ~ /[<>]=? *[^;]*[0-9]+\.[0-9]+/)) { hit[++k] = file SUBSEP n }
    n++
}
END {
    for (i = 1; i <= k; i++) {
        split(hit[i], h, SUBSEP)
        if (!((h[1], h[2]) in seen) && !((h[1], h[2] - 1) in seen) && !((h[1], h[2] + 1) in seen)) {
            printf "%s:%d: band without \"// measured: <value>, band: <factor>\"\n", h[1], h[2]; bad = 1
        }
    }
    exit bad
}
' || bad=1

# 4 and 5. Raw conversion factors and core formulas only in their helper.
added 'Source/UltraGravity/*.h' 'Source/UltraGravity/*.cpp' ':!Source/UltraGravity/ThirdParty' 'ultragravity/*.h' 'ultragravity/*.cpp' ':!ultragravity/third_party' | awk "$track"'
/^\+/ {
    line = substr($0, 2)
    units = "Source/UltraGravity/Public/ultragravity/units.h"
    if (file != units && (line ~ /[\/*] *3\.6([^0-9]|$)/ || (line ~ /pi/ && line ~ /\/ *60([^0-9]|\.0|$)/))) {
        printf "%s:%d: use the conversion kmh / rpm from ultragravity/units.h\n", file, n; bad = 1
    }
    if (line !~ /helper-ok/) {
        if (file != "Source/UltraGravity/Public/ultragravity/vector_math.h" && line ~ /atan2\(/ && line ~ /\.y/) {
            printf "%s:%d: drift angle: use drift_angle / drift_angle_full / drift_angle_magnitude from vector_math.h (docs/agents/helpers.md)\n", file, n; bad = 1
        }
        if (file != units && line ~ /9\.81/) {
            printf "%s:%d: use gravity from ultragravity/units.h\n", file, n; bad = 1
        }
    }
    n++
}
END { exit bad }
' || bad=1

# 6. Integer fields in headers carry a unit or meaning comment.
added 'Source/UltraGravity/Public/*.h' 'ultragravity/*.h' ':!ultragravity/third_party' | awk "$track"'
/^\+/ {
    line = substr($0, 2)
    if (line ~ /^[ \t]*(std::optional<(int|double)>|int|std::size_t) [A-Za-z_0-9]+ ?(=|;|\{)/ && line !~ /\/\//) {
        printf "%s:%d: comment missing (// 1, PP, ...)\n", file, n; bad = 1
    }
    n++
}
END { exit bad }
' || bad=1

exit "${bad:-0}"
