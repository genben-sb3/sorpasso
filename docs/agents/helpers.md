# Geteilte Helfer

Vor jeder neuen Formel hier nachsehen und mit `rg` nach ihrem Kernausdruck suchen. Wer einen Helfer neu anlegt, ergänzt hier eine Zeile im selben Commit. Hat er einen eindeutigen Kernausdruck, kommt das Muster in `.githooks/check-units.sh` (Block 5).

- `kmh`, `rpm`: Umrechnungsfaktoren, `Source/UltraGravity/Public/ultragravity/units.h`
- `gravity`, `lateral_acceleration_limit(peak_friction)`: `Source/UltraGravity/Public/ultragravity/units.h`
- `overall_ratio(Gearbox, gear)`: `Source/UltraGravity/Public/ultragravity/vehicle_file.h` (negativ im Rückwärtsgang −1, 0 im Leerlauf)
- `drift_angle_magnitude(Vec3)` (Betrag), `drift_angle(Vec3, longitudinal_min)` (mit Vorzeichen, geklemmt), `drift_angle_full(Vec3)` (mit Vorzeichen, voller Winkelbereich): `Source/UltraGravity/Public/ultragravity/vector_math.h`
- `wrap_angle`, `interpolate` (Animationsdaten): `Source/UltraGravity/Public/ultragravity/animation_data.h`
- `slip_ratio`, `wheel_speed_at_slip` (Umkehrung), `slip_denominator`: `Source/UltraGravity/Private/vehicle_step.cpp`
- `ground_speed` (Lambda in der Schaltlogik der Automatik): `Source/UltraGravity/Private/vehicle_step.cpp`
- Magic Formula, `peak_slip`: `Source/UltraGravity/Private/magic_formula.h`
- `compute_pp`, `physics_version_id`, `set_pp`: `ultragravity/tuning/include/ultragravity/pp_value.h`
