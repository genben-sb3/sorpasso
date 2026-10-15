# Recherche: Eigene Fahrphysik in C++ auf Unreal Engine 5 (SORPASSO)

> Stand: 2026-10-11. Kontext: [`DESIGN.md`](../DESIGN.md), Abschnitte Fahrphysik, Steuerung, Multiplayer und M1.
> Ziel: Grundlage für die Spec von **M1 „Fahrgefühl“**. Eigenes Fahrzeugmodell, nicht Chaos Vehicles. Unreal liefert nur Rigid Body und Kollision.

**Wie die Quellen gelesen werden:**

- Jede Aussage hat einen Link. Bevorzugt wurden Primärquellen: Epic-Dokumentation/API, Originalpaper (SAE), MathWorks-Referenzdoku und der **Quellcode** von VDrift, Speed Dreams und Rigs of Rods. Den Code habe ich selbst gelesen, Links gehen auf feste Commits mit Zeilennummern.
- **[unverifiziert]** heißt: Ich konnte die Primärquelle nicht einsehen (Paywall, offline, nur Sekundärquelle) oder die Aussage ist meine eigene Ableitung.
- **[Empfehlung]** heißt: eigene Engineering-Einschätzung, keine belegte Tatsache.
- Gelesene Code-Stände:
  - VDrift `f835edae` (2026-04-07)
  - Speed Dreams `a5c33441` (2026-10-07)
  - Rigs of Rods `d426a5c5`

Basis-URLs der Code-Zitate:

- VDrift: `https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/…`
- Speed Dreams: `https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/…`

---

## 1. Reifenmodell

### 1.1 Pacejka Magic Formula: Grundform

- **Herkunft:** Die Magic Formula geht auf Bakker, Nyborg und Pacejka zurück: *Tyre Modelling for Use in Vehicle Dynamics Studies*, SAE 870421, 1987 ([SAE](https://saemobilus.sae.org/papers/tyre-modelling-use-vehicle-dynamics-studies-870421)). Laut Abstract beschreibt die Formel Seitenkraft, Bremskraft und Rückstellmoment bei reinem Schräglauf bzw. reinem Bremsen. Die Koeffizienten haben eine direkte Bedeutung, etwa Schlupfsteifigkeit bei null Schlupf und Kraftmaxima. Kombinierter Schlupf baut darauf auf.
- **Nachfolger:** SAE 890087 (1989), „Pac89“ ([Wikipedia-Zusammenfassung, sekundär](https://en.wikipedia.org/wiki/Hans_B._Pacejka)).
- **Standardwerk:** Pacejka & Besselink, *Tire and Vehicle Dynamics*, 3. Aufl., Elsevier 2012 ([Verlag](https://shop.elsevier.com/books/tire-and-vehicle-dynamics/pacejka/978-0-08-097016-5), [Begleitseite mit MATLAB-Programmen](https://booksite.elsevier.com/9780080970165)). Den Volltext konnte ich nicht einsehen. Alles, was ich unten dem Buch zuschreibe, ist **[unverifiziert]** bis jemand es nachschlägt.

Die Grundform mit konstanten Koeffizienten (so dokumentiert von [MathWorks, Tire-Road Interaction (Magic Formula)](https://www.mathworks.com/help/sdl/ref/tireroadinteractionmagicformula.html)):

```
F = Fz · D · sin( C · atan( B·x − E·(B·x − atan(B·x)) ) )
x = Schlupf (κ längs bzw. α quer)
B = Steifigkeit, C = Form, D = Peak (≈ μ), E = Krümmung
```

**Typische Koeffizienten** (konstante Sätze, MathWorks, [Quelle](https://www.mathworks.com/help/sdl/ref/tireroadinteractionmagicformula.html)):

| Belag | B | C | D | E |
|---|---|---|---|---|
| Trockener Asphalt | 10 | 1.9 | 1 | 0.97 |
| Nasser Asphalt | 12 | 2.3 | 0.82 | 1 |
| Schnee | 5 | 2 | 0.3 | 1 |
| Eis | 4 | 2 | 0.1 | 1 |

Hinweise dazu:

- Die Werte gelten für den **Längsschlupf κ** (dimensionslos). Für den Querschlupf in Radiant braucht man ein eigenes B.
- Als Startwert für einen Straßen-/Sportreifen: D ≈ 1.0–1.1. VPP zitiert den Assetto-Corsa-Entwickler mit „a typical road tire will give you 1.0 - 1.1 G“ ([VPP Misc Topics](https://vehiclephysics.com/advanced/misc-topics-explained/); das ist ein Zitat aus zweiter Hand, **[unverifiziert]** beim Original).
- Peak-Schlupf von etwa 0.08–0.15 (längs) und etwa 5–10° (quer) bei Straßenreifen ist ein verbreiteter Faustwert. Eine Primärquelle habe ich dafür nicht gefunden: **[unverifiziert]**.

**Lastabhängige Form (Pac89/94-Stil)**, so wie VDrift sie implementiert ([`cartire1.cpp` L232–265](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cartire1.cpp#L232-L265)):

```
D   = (b1·Fz + b2)·Fz                 // Peak, wächst unterlinear mit der Last → Lastempfindlichkeit
BCD = (b3·Fz + b4)·Fz·exp(−b5·Fz)      // Schlupfsteifigkeit bei σ=0
B   = BCD / (C·D)
E   = (b6·Fz + b7)·Fz + b8
S   = 100·σ + Sh                       // Pac94 rechnet Längsschlupf in Prozent!
```

Die Querkraft funktioniert analog, zusätzlich mit Sturz γ und vertikalem Shift Sv ([L268–305](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cartire1.cpp#L268-L305)).

**Achtung Einheiten:** In Pac94 ist der Schräglaufwinkel in **Grad**, der Längsschlupf in **Prozent** und Fz in **kN** angegeben. VDrift rechnet deshalb `alpha = slip_angle * rad2deg` und `100 * sigma` ([L68–70](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cartire1.cpp#L68-L70)). Die Referenz für die b0..b13/a0..a17-Sätze war früher die Racer-Doku von Ruud van Gaal (racer.nl). Die Domain zeigt heute eine fremde Seite: **[nicht mehr verfügbar]**.

**Lastempfindlichkeit, einfach:** Speed Dreams skaliert μ mit `lfMin + (lfMax − lfMin)·exp(lfK·Fz/opLoad)` ([`wheel.cpp` L735f.](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/wheel.cpp#L735)). Für das Fahrgefühl ist das zentral. Mehr Last bringt unterproportional mehr Kraft. Erst dadurch kosten Gewichtsverlagerung und Rollsteifigkeitsverteilung Grip, und genau darüber stimmt man Über- und Untersteuern ab.

### 1.2 Kombinierter Schlupf

Drei belegte Ansätze:

1. **MF-Gewichtungsfunktionen** (VDrift):
   - Formel: `Fx = Gx(σ,α)·Fx0`, `Fy = Gy(σ,α)·Fy0` ([`cartire1.cpp` L78–82](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cartire1.cpp#L78-L82)).
   - Dabei ist `G = cos(atan(B·x))` mit B selbst schlupfabhängig. VDrift rechnet das ohne Trigonometrie als `sqrt(c/(c+b²))` ([L347–370](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cartire1.cpp#L347-L370)).
   - Die allgemeine Form `G = cos(C·atan(B·x))` stammt aus Pacejka 2012, Kap. 4 **[unverifiziert, Buch nicht eingesehen]**.
2. **Normierter Schlupfvektor** (Speed Dreams/TORCS-Linie):
   - Ein Schlupfvektor `(sx, sy)` mit `sy = sin(sa)`, Betrag `s`. Die Magic Formula wird einmal auf `s` ausgewertet, dann nach `sx/s` und `sy/s` aufgeteilt ([`wheel.cpp` L731–783](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/wheel.cpp#L731-L783)).
   - Ergibt automatisch einen Reibkreis und ist sehr robust, aber weniger realistisch.
3. **Reibkreis/„Traction Budget“** als Begriff: Beckman, *The Physics of Racing*, Part 7 ([Spiegel](https://cygnus-supra.duckdns.org/Supra/Library/PhysicsOfRacing/physics_of_racing_07.html), Original © 1991). Die Gesamthaftung wird auf Bremsen, Beschleunigen und Kurve aufgeteilt.

**Edy's Vehicle Physics Pro (VPP)** geht einen anderen Weg und rechnet mit **Schlupfgeschwindigkeit in m/s** statt Schlupfverhältnis und -winkel. Die Kurve hat drei Bereiche: Adherent, Peak und Limit ([VPP Tires](https://vehiclephysics.com/blocks/tires/)). Laut Doku sind „Standard Pacejka coefficients … not directly compatible with VPP“.

### 1.3 Singularität bei niedriger Geschwindigkeit

Problem:

- Schlupfverhältnis `κ = (ωr − vx)/|vx|` und Schlupfwinkel `α = atan(vy/|vx|)` teilen durch `|vx|` und divergieren bei Stillstand. Definition: [MathWorks Longitudinal Wheel](https://www.mathworks.com/help/autoblks/ref/longitudinalwheel.html); Marco Monster definiert es gleich ([Spiegel von „Car Physics for Games“](https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html)).
- Dazu kommt Steifigkeit: Eine hohe Schlupfsteifigkeit mal einem winzigen Nenner ergibt bei explizitem Euler Schwingungen und Zittern im Stand.

Belegte Lösungen:

| Ansatz | Wer | Beleg |
|---|---|---|
| Nenner klemmen: `1/max(|vx|, 1e-3)` | VDrift | [`cartirebase.h` L23–31](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cartirebase.h#L23-L31) |
| Nenner auf 1 m/s klemmen („avoid divergence“), Option `FEAT_SLOWGRIP` | Speed Dreams | [`wheel.cpp` L687](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/wheel.cpp#L687) |
| Nenner **glatt** auf `VXLOW` sättigen | MathWorks | [Tire-Road Interaction](https://www.mathworks.com/help/sdl/ref/tireroadinteractionmagicformula.html): „The block saturates the slip denominator … smoothly transitions \|Vx\| to VXLOW“ |
| Kraft zeitlich filtern (Tiefpass `prev + 0.5·(target − prev)`) | Speed Dreams, ohne SLOWGRIP | [`wheel.cpp` L791](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/wheel.cpp#L791), Makro [`robottools.h` L50](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/libs/robottools/robottools.h#L50) |
| **Schlupf als Zustandsgröße** (Differentialgleichung statt kinematischer Formel); behandelt Anhalten „without numerical problems“ | Bernard & Clover, SAE 950311 (1995) | [SAE-Abstract](https://saemobilus.sae.org/papers/tire-modeling-low-speed-high-speed-calculations-950311) (Volltext **[unverifiziert]**) |
| **Reifenkraft als Constraint:** Die Pacejka-Kraft ist nur die Impulsgrenze einer Geschwindigkeits-Constraint „Schlupfgeschwindigkeit → 0“ | VDrift | [`cardynamics.cpp` L1443–1447](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1443-L1447) |
| „Adherent“-Haftreibung bei kleiner Schlupfgeschwindigkeit, ausgeblendet unterhalb `maxAdherentSpeed` (≈ 23 km/h) | VPP | [VPP Tires](https://vehiclephysics.com/blocks/tires/) |
| Bei niedriger Geschwindigkeit ist es ein Kinematik-Problem: „the wheels pretty much roll in the direction they're pointed at“ | Marco Monster | [Spiegel](https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html) |

**Bewertung:** Der VDrift-Constraint-Ansatz ist am elegantesten. Ein Constraint kann nie „überschießen“: Bei kleinem Schlupf liefert er genau so viel Kraft, wie nötig ist, um den Schlupf aufzuheben, höchstens aber die Pacejka-Kraft. Das löst Stillstand und Steifigkeit zugleich. Für M1 ist er aber aufwendiger als ein expliziter Ansatz. **[Empfehlung]:** Siehe Abschnitt 9.

### 1.4 Relaxationslänge

- **Definition:** Die Relaxationslänge ist die Rollstrecke, nach der die Seitenkraft etwa 63 % ihres stationären Werts erreicht ([MathWorks Combined Slip Wheel 2DOF](https://in.mathworks.com/help/vdynblks/ref/combinedslipwheel2dof.html), sinngemäß).
- **Gleichung:** MathWorks [Vehicle Body 3DOF](https://www.mathworks.com/help/vdynblks/ref/vehiclebody3dof.html) dokumentiert ein Verzögerungsglied erster Ordnung: `α_σ = 1/s·[(α − α_σ)·v_w/σ]`, also
  ```
  dα'/dt = (|v| / σ) · (α − α')     // σ = Relaxationslänge [m], v = Radgeschwindigkeit
  ```
  Der Nenner ist auf der Seite offenbar falsch gerendert. Ich lese ihn als σ; das ist meine Interpretation.
- **Längs:** MathWorks Longitudinal Wheel nutzt eine Relaxationslänge mit **Default 0.5 m** ([Quelle](https://www.mathworks.com/help/autoblks/ref/longitudinalwheel.html)).
- **Faustregel nach Pacejka:** Die Relaxationslänge liegt bei Nennlast in der Größenordnung des Radradius; gemessen wurden 0.12–0.45 m ([Wikipedia](https://en.wikipedia.org/wiki/Relaxation_length), sekundär, **[unverifiziert]** im Buch).
- **Nutzen für uns:**
  1. Sie dämpft das Zittern bei niedriger Geschwindigkeit.
  2. Die Zeitkonstante `σ/|v|` wird bei hoher Geschwindigkeit klein. Dann muss der Zeitschritt sie auflösen. Bei 300 km/h und σ = 0.3 m sind das etwa 3.6 ms, ein Argument für ≥ 500 Hz (eigene Rechnung).
  3. Bei v → 0 wird die Zeitkonstante unendlich, deshalb braucht `|v|` eine Untergrenze **[Empfehlung]**.
- **VPP:** „Low side deflection rates at high speeds can enter in resonance with lateral forces destabilizing the vehicle“ ([VPP Misc Topics](https://vehiclephysics.com/advanced/misc-topics-explained/)).

---

## 2. Fahrwerk und Gewichtsverlagerung

- **Raycast pro Rad:** Das ist der Standard in Spielen.
  - Ein UE5-Praxisbericht modelliert Federbeine als Line Trace vom oberen Anlenkpunkt, mit Länge = Federweg + Radradius ([Grenoville, Medium/levelup](https://levelup.gitconnected.com/taming-chaos-stable-vehicle-suspensions-with-async-physics-in-ue5-566369c7b097); nur über die Suchzusammenfassung, die Seite gab 403 zurück: **[unverifiziert]**).
  - Ein Shape-Cast (Kugel oder Zylinder) glättet Kanten und Curbs besser als ein Strahl **[Empfehlung]**.
- **Feder/Dämpfer:** `F = k·x + c·ẋ`, mit Bump-Stop. VDrift addiert bei Überweg eine Anschlagsteifigkeit von 5e5 N/m ([`cardynamics.cpp` L1358](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1358)). Dämpfung getrennt nach Druck- und Zugstufe ist üblich **[Empfehlung]**.
- **Stabilisator:** VDrift addiert pro Achse eine Zusatzsteifigkeit proportional zur Wegdifferenz links/rechts. Negative Gesamtsteifigkeit wird verhindert ([`cardynamics.cpp` L1328–1346](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1328-L1346)). Äquivalent und einfacher: `F_arb = k_arb·(x_l − x_r)`, wirkt +F links und −F rechts.
- **Last auf den Reifen:**
  - Fz ist die **Federkraft aus dem Raycast** (Feder + Dämpfer + Stabi), auf ≥ 0 geklemmt. Gewichtsverlagerung entsteht dann **von selbst** aus der Starrkörperbewegung (Nicken, Wanken). Eine Formel braucht man dafür nicht.
  - VDrift nimmt Fz direkt aus dem Suspension-Constraint-Impuls (`fz = impulse * rdt`, [L1511](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1511)).
- **Analytische Formel zur Kontrolle:**
  - Beckman: `Lf = d·G + B·h/w`, `Lr = (1−d)·G − B·h/w` (G = Gewicht, B = Bremskraft, h = Schwerpunkthöhe, w = Radstand) ([Physics of Racing Part 1, Spiegel](https://cygnus-supra.duckdns.org/Supra/Library/PhysicsOfRacing/physics_of_racing_01.html), © 1991).
  - Marco Monster nutzt dasselbe in der Form `Wf = (c/L)·W − (h/L)·M·a` ([Spiegel](https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html)).
  - Nutzbar als **Unit-Test** für das Raycast-Fahrwerk im stationären Zustand **[Empfehlung]**.
- **Wo die Kraft angreift:** VPP warnt, dass Reifenkräfte am Kontaktpunkt statt in Schwerpunkthöhe ein unerwünschtes Rollmoment erzeugen. Das gleicht VPP mit `tireImpulseRatio` aus ([VPP VehicleBase](https://vehiclephysics.com/advanced/vehiclebase-reference/)). Im Spiel greift man die Kraft oft etwas oberhalb des Kontaktpunkts an, um das Rollen zu dämpfen **[unverifiziert/Empfehlung]**.

---

## 3. Antriebsstrang

- **Motor:**
  - Drehmomentkurve als Lookup-Tabelle mit linearer Interpolation, skaliert mit dem Gaspedal ([Marco Monster](https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html)).
  - Monster klemmt die Drehzahl nach unten, „needed to get the car into motion from a standstill“.
  - Besser ist eine eigene Motorträgheit mit Leerlaufregler und Schleppmoment (Motorbremse) **[Empfehlung]**.
- **Kraft am Rad (stationär):** `F = T_engine·x_g·x_d·η / r_w` ([Marco Monster](https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html)). Das taugt nur als Plausibilitätscheck: Ein Modell mit starr gekoppelter Drehzahl hat kein Durchdrehen und keinen Kupplungsschlupf.
- **Stabile Kopplung Motor ↔ Räder:** Das ist das eigentliche Problem. Ein Motor mit kleiner Trägheit, über die Übersetzung (i² vergrößert die reflektierte Trägheit) starr an die Räder gekoppelt, ergibt mit expliziter Integration ein steifes System.
  - **VDrift** löst den ganzen Antriebsstrang als **Constraints** (sequentielle Impulse). Motor und Bremsen sind `MotorJoint` (Zielgeschwindigkeit, Impulsgrenze = Drehmoment·dt), Kupplung und Differenziale sind `ClutchJoint` mit drehmomentbegrenzter Kopplung ([`driveline.h` L28, L68ff.](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/driveline.h#L28)).
  - Gelöst wird in **10 Substeps × 4 Iterationen** pro 90-Hz-Physikschritt, verschränkt mit den Reifen-Constraints ([`cardynamics.cpp` L40, L1452–1506](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1452-L1506); Zeitschritt `1/90.0` in [`game.cpp` L96](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/game.cpp#L96)). Effektiv also etwa 900 Hz für Räder und Antriebsstrang.
  - **Speed Dreams** koppelt Motor und Achse über eine geglättete „Joint-Trägheit“ (Tiefpass α = 0.1). Kommentar im Code: „limit the difference to avoid model instability“ ([`engine.cpp` ab L330](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/engine.cpp#L330)). Das zeigt: Ein explizites Modell braucht hier Hilfskrücken.
  - **VPP:** mindestens 2 Substeps, „Multi-driven-axle vehicles should configure at least two substeps per driven axle“; 20 Substeps bei 50 Hz ergeben 1000 Hz intern ([VehicleBase](https://vehiclephysics.com/advanced/vehiclebase-reference/), [Misc Topics](https://vehiclephysics.com/advanced/misc-topics-explained/)).
- **Kupplung:** Sie überträgt maximal `T_max = μ·F_press·r` (Coulomb). Haftet sie (Schlupf ≈ 0), werden Motor und Getriebeeingang zu einer Trägheit zusammengelegt (implizit oder per Constraint). Sonst überträgt sie `T_max·sign(Δω)` **[Empfehlung, Standardmechanik]**. VDrift modelliert genau diese Begrenzung als Impulsgrenze ([`driveline.h`](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/driveline.h#L68)).
- **Getriebe:** Übersetzungen, Schaltzeit (Kupplung offen bzw. Momentenunterbrechung). VDrift nutzt `remaining_shift_time` ([`cardynamics.cpp` L1772ff.](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1772)).
- **Differenziale:** Speed Dreams kennt `FREE`, `LIMITED_SLIP`, `VISCOUS_COUPLER`, `SPOOL` und `15WAY_LSD` ([`differential.cpp` L92ff.](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/differential.cpp#L92)).
  - Deren LSD: Die Sperrwirkung wächst mit dem Eingangsmoment (`rate = 1 − exp(−(T/lockInputTq)²)`), die Momentenverteilung verschiebt sich über `tanh(rate·Δω)` ([L301–330](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/differential.cpp#L301-L330)).
  - VDrift modelliert das LSD als drehmomentbegrenzte Kupplung zwischen den Halbwellen (`anti_slip`, `anti_slip_torque`, `decel_factor`, [`cardynamics.cpp` L1253–1256](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1253-L1256)). Das ist Preload plus lastabhängige Sperre, also ein klassisches Lamellen-LSD (1-, 1.5- und 2-Wege über `decel_factor`).
  - VPP: „viscous couplings … should be always integrated at a given rate“ ([Misc Topics](https://vehiclephysics.com/advanced/misc-topics-explained/)).
- **Radträgheit:** `I·dω/dt = T_drive − T_brake − Fx·r`. Bei explizitem Euler und kleiner Radträgheit ist das die steifste Gleichung im System. Bremsen blockieren deshalb erst richtig, wenn das Bremsmoment als **begrenzter Impuls Richtung ω = 0** umgesetzt wird. VDrift macht das so: Bremsen sind `MotorJoint` mit Zielgeschwindigkeit 0 ([`driveline.h` L28–56](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/driveline.h#L28-L56)). Ein naives `ω −= T_brake/I·dt` schwingt um null.

---

## 4. Aerodynamik

- **Luftwiderstand:** `F_drag = ½·ρ·Cd·A·v²`. Marco Monster nutzt Cd = 0.30, A = 2.2 m², ρ = 1.29 kg/m³, also `Cdrag ≈ 0.4257` ([Spiegel](https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html)).
- **Rollwiderstand:** Monster setzt `Crr ≈ 30·Cdrag`, gibt aber zu: „I couldn't confirm its value anywhere“. Besser ist Rollwiderstand pro Rad proportional zu Fz. Speed Dreams macht das als `rollRes = Fz·kRollRes` des Belags ([`wheel.cpp`](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/wheel.cpp#L774)).
- **Abtrieb:** `F_down = ½·ρ·Cl·A·v²`, **pro Achse** an der Achsposition angreifen lassen, damit die Aero-Balance stimmt. VDrift hat dafür ein eigenes `aerodevice.h` und nutzt Abtrieb sogar in `GetMaxSpeed`: `m·v²/r = μ·(m·g + cl·v²)` ([`cardynamics.cpp` L1006ff.](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1006)). Das ist auch für die KI-Kurvengeschwindigkeit in M2 nützlich.
- Für M1 reichen Widerstand und ein kleiner Abtrieb pro Achse. Ground Effect und Abhängigkeit von der Fahrzeughöhe kommen später **[Empfehlung]**.

---

## 5. Fester Zeitschritt und Substepping in UE5

### 5.1 Was Unreal anbietet (Primärquellen Epic)

- **Projekteinstellungen** ([UPhysicsSettings API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UPhysicsSettings)):

  | Einstellung | Bedeutung |
  |---|---|
  | `bTickPhysicsAsync` | „Whether to tick physics simulation on an async thread.“ |
  | `AsyncFixedTimeStepSize` | „If using async, the time step size to tick at.“ |
  | `bSubstepping` / `bSubsteppingAsync` | Substepping an/aus, auch für die Async-Simulation |
  | `MaxSubstepDeltaTime` | Länge eines Substeps, geklemmt auf **0.0013–1.0 s** (also max. ca. 770 Hz) |
  | `MaxSubsteps` | Anzahl Substeps, 1–16 |
  | `MaxPhysicsDeltaTime` | Obergrenze pro Schritt |

  Async Physics ist laut Doku **experimentell** (Hinweis in den Suchergebnissen zu Project Settings, [Physics Settings](https://dev.epicgames.com/documentation/unreal-engine/physics-settings-in-the-unreal-engine-project-settings)).
- **Substepping-Doku** ([Physics Sub-Stepping](https://dev.epicgames.com/documentation/en-us/unreal-engine/physics-sub-stepping-in-unreal-engine)):
  - Substepping ist für den Nutzer verdeckt. Kräfte, die zwischen Frames gesetzt werden, kompensiert die Engine selbst: „we need to apply the force for N consecutive simulation steps to achieve the same acceleration.“
  - Kollisions-Callbacks kommen erst nach dem letzten Substep.
  - Laut Seite „work in progress“.
  - **Folge für uns:** Mit klassischem Substepping kann man Reifenkräfte **nicht** pro Substep neu berechnen. Sie bleiben über den ganzen Frame konstant. Für ein Fahrzeugmodell taugt das nicht.
- **`UActorComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)`**: „Override this function to implement custom logic to be executed every physics step“. Läuft nur, wenn `bAsyncPhysicsTick` gesetzt ist ([API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UActorComponent/AsyncPhysicsTickComponent)).
  - **Wichtig**, laut [SetAsyncPhysicsTickEnabled](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UActorComponent/SetAsyncPhysicsTickEnabled): „These functions will execute on the game thread … allowed to read and write both Physics Thread (PT) and Game Thread (GT) Data“, plus „Performance Warning: This will cause both threads to halt and force a full sync … The PT will block until the GT is done executing all AsyncPhysicsTick functions.“
  - **Folge:** Einfach zu benutzen, World-Traces sind vermutlich erlaubt (Game Thread). Dafür synchronisiert es beide Threads bei jedem Physikschritt.
  - Ein Drittanbieter-Blog behauptet das Gegenteil, nämlich Ausführung auf dem Physik-Thread. Die Epic-Doku hat Vorrang. Vor der Festlegung mit der gepinnten Engine-Version prüfen **[offen]**.
- **Kräfte im Async-Tick:** `UPrimitiveComponent::GetBodyInstanceAsyncPhysicsTickHandle` liefert ein `FBodyInstanceAsyncPhysicsTickHandle`. Darauf arbeiten `UPhysicsThreadLibrary::AddForce` und Verwandte: „should be called every frame for the duration of the force“ ([AddForce](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UPhysicsThreadLibrary/AddForce), [Handle](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FBodyInstanceAsyncPhysicsTickHan-)).
- **Low-Level-Variante, echter Physik-Thread:** `Chaos::TSimCallbackObject<TInput, TOutput, Options>` (`Chaos/SimCallbackObject.h`), registriert über `FPhysicsSolverBase::CreateAndRegisterSimCallbackObject_External`. Input kommt per lock-free Queue vom Game Thread (`GetProducerInputData_External` / `GetConsumerInput_Internal`), Output geht zurück (`PopOutputData_External`) ([TSimCallbackObject](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Chaos/TSimCallbackObject), [CreateAndRegister…](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Chaos/FPhysicsSolverBase/CreateAndRegisterSimCallbackObje-)).
  - Der Hook `OnPreSimulate_Internal` heißt laut (4.27-)Doku „Called before simulation step“ ([FChaosSceneSimCallback::OnPreSimulate_Internal](https://docs.unrealengine.com/4.27/en-US/API/PhysicsCore/Chaos/FChaosSceneSimCallback/OnPreSimulate_Internal/index.html)).
  - **Epics eigene Chaos Vehicles nutzen genau dieses Muster:** `FChaosVehicleManagerAsyncCallback` ist laut API-Doku eine abgeleitete Klasse von `TSimCallbackObject` ([TSimCallbackObject, „Derived classes“](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Chaos/TSimCallbackObject)).
  - Nachteil: Auf dem Physik-Thread sind Game-Thread-Objekte und normale World-Traces tabu. Die Fab-Listing-Warnung zu DrawDebug ist sekundär, die genauen Scene-Query-Regeln sind **[unverifiziert]**.
- **Networked Physics / Resimulation:** Physics Prediction setzt „Tick Physics Async“ voraus und synchronisiert Tick-Nummern zwischen Client und Server ([Networked Physics Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/networked-physics-overview)). Für später relevant, weil das dieselbe Architektur ist (Eingaben pro Physik-Tick).
- **Bekanntes Problem, Forenbericht ohne Epic-Bestätigung:** Bei aktivem Async Physics werden Körper langsamer, wenn die Game-Thread-FPS unter `MaxPhysicsDeltaTime` fallen ([Forum](https://forums.unrealengine.com/t/async-physics-slows-down-at-low-game-thread-fps/2731575)) **[unverifiziert]**.

### 5.2 Rechenfrequenzen anderer Simulationen (aus dem Code belegt)

| Sim | Rate | Beleg |
|---|---|---|
| VDrift | Welt 90 Hz, Räder und Antriebsstrang 10 Substeps (≈ 900 Hz) | [`game.cpp` L96](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/game.cpp#L96), [`cardynamics.cpp` L40](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L40) |
| Speed Dreams / TORCS-Linie | 500 Hz (`RCM_MAX_DT_SIMU 0.002`) | [`raceman.h` L54](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/interfaces/raceman.h#L54) |
| Rigs of Rods (Soft-Body, Knoten und Balken) | 2000 Hz (`PHYSICS_DT 0.0005f`) | [`SimConstants.h` L20](https://github.com/RigsOfRods/rigs-of-rods/blob/d426a5c5e9f76a77a34e5fba8a41cda20edbe97f/source/main/physics/SimConstants.h#L20) |
| VPP (Unity) | Unity 50 Hz × bis zu 20 Substeps = 1000 Hz intern | [VPP Misc Topics](https://vehiclephysics.com/advanced/misc-topics-explained/) |

Das typische Muster: **Starrkörper relativ langsam (50–120 Hz), Räder/Reifen/Antriebsstrang intern schnell (500–1000 Hz).**

### 5.3 Determinismus (Replays/Ghosts)

- **Chaos:** Eine offizielle Epic-Aussage, dass Chaos deterministisch ist, habe ich nicht gefunden. Ein älterer Forenbeitrag gibt sinngemäß wieder, dass keine deterministische Physik-Engine geplant sei ([Forum](https://forums.unrealengine.com/t/chaos-physics-determinism/151350), Hörensagen) **[unverifiziert]**.
- **VDrift-Replays sind hybrid:** Eingabe-Deltas („p-frames“) plus periodische **vollständige Fahrzeugzustände** („i-frames“) ([`replay.h` L94–135](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/replay.h#L94-L135)). Kleine Abweichungen werden dadurch regelmäßig zurückgesetzt.
- **[Empfehlung]:**
  - Eingabe-Pakete pro Sim-Tick aufzeichnen (wie DESIGN.md verlangt) **und** alle N Ticks einen Zustandsschnappschuss (Transform, Geschwindigkeiten, Raddrehzahlen, Motordrehzahl, Gang). Für **Ghosts** reicht sogar die reine Zustandsaufzeichnung, weil ein Ghost nicht kollidiert.
  - Unser eigener Fahrzeugcode sollte trotzdem deterministisch bei gleichem Binary sein: fester dt, keine Abhängigkeit von Frame-Zeit, keine unsortierten Container, kein `FMath::Rand` ohne Seed. So lassen sich Physik-Regressionen per Golden-Run-Test erkennen.

---

## 6. Fahrhilfen (ABS, TCS, ESC)

Grundsatz aus DESIGN.md: Die Physik bleibt unverändert. Fahrhilfen sind ein **Steuergerät** zwischen Eingabe-Paket und Aktoren (Bremsdruck pro Rad, Drosselklappe). Genau so ist es in allen gelesenen Quellen umgesetzt.

- **ABS:**
  - **VDrift** arbeitet prädiktiv. Es schätzt den Schlupf im nächsten Schritt (`slip + (slip − slip_prev)`), normiert ihn auf den Peak-Schlupf aus einer lastabhängigen Tabelle (`ideal_slip`) und begrenzt die Änderungsrate des Bremsfaktors. Aktiv erst ab 4 m/s ([`ApplyABS` L1124–1148](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1124-L1148)). Die Peak-Schlupf-Tabelle wird numerisch aus der Pacejka-Kurve bestimmt (`FindPacejkaMax`).
  - **Speed Dreams** macht es einfacher: Liegt der Schlupf über der Schwelle, wird das Bremsmoment mit `1 − k·sx` skaliert ([`wheel.cpp` L850](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/wheel.cpp#L850), angewendet in [`brake.cpp` L63](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/brake.cpp#L63)).
- **TCS:**
  - **VDrift** regelt mit Hysterese: Eingriff bei Schlupf > 1.2 × Ideal, Ende bei < 0.8 × Ideal. Das Gas wird proportional zum Fehler reduziert, der Idealwert sinkt mit wachsendem Schräglauf (`ideal·cos(...)`) ([`ApplyTCS` L1094–1122](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1094-L1122)).
  - **Speed Dreams** bremst zusätzlich das am stärksten durchdrehende Rad (`TCL_BrakeScale`, [`brake.cpp` L70ff.](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/brake.cpp#L70)).
  - **VPP** unterscheidet Modi: „Street“ hält den Reifen im Haftbereich, „Sport“ zielt auf Peak-Schlupf ([VPP](https://vehiclephysics.com/advanced/misc-topics-explained/)). Das entspricht den GT6-Stufen 1…5 sinngemäß **[Empfehlung: TCS-Stufe = Ziel-Schlupf-Faktor]**.
- **ESC:** Speed Dreams nutzt den Driftwinkel (Fahrtrichtung minus Gierwinkel). Über 7.5° werden diagonal einzelne Räder gebremst (vorne außen/innen gegenläufig, hinten zusätzlich Balance) ([`brake.cpp` L130–190](https://forge.a-lec.org/speed-dreams/speed-dreams-code/src/commit/a5c3344149ad369c530a9e43e83461a28a7ec2ad/src/modules/simu/simuv5/brake.cpp#L130-L190)).
  - Realistischer ist der Vergleich der **Soll-Gierrate** (Einspurmodell: `r_ref = v·δ / (L·(1 + K·v²))`, begrenzt auf `μ·g/v`) mit der **Ist-Gierrate**. Bei Übersteuern wird das kurvenäußere Vorderrad gebremst, bei Untersteuern das kurveninnere Hinterrad, und zusätzlich das Gas reduziert. Das ist Lehrbuchstandard, hier aber **[unverifiziert, keine Primärquelle gelesen]**.

---

## 7. Eingabe-Glättung für Tastatur und Gamepad

- **Rate Limiting:**
  - **VDrift** führt bei Nicht-Lenkrad-Eingaben die Lenkposition mit maximal `5·dt` pro Schritt der Zieleingabe nach, also volle Lenkung in 0.2 s ([`carcontrolmap.cpp` L831ff.](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/carcontrolmap.cpp#L831)).
  - Unreals eigene Chaos Vehicles haben dafür `FVehicleInputRateConfig` mit getrennter **RiseRate/FallRate** plus `InputCurveFunction` bzw. User-Curve ([API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ChaosVehicles/FVehicleInputRateConfig)).
  - Getrennte Raten sind sinnvoll: Die Rückstellung (Fall) darf schneller sein als der Aufbau (Rise) **[Empfehlung]**.
- **Geschwindigkeitsabhängige Lenkung:**
  - **VDrift** skaliert den Lenkausschlag mit einer atan-Kurve über die Geschwindigkeit, gewichtet mit `(1 − val²)` ([`carcontrolmap.cpp` L807–822](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/carcontrolmap.cpp#L807)).
  - Die **Steering Assist** von VDrift begrenzt den Lenkwinkel bei hoher Geschwindigkeit auf den **Peak-Schräglaufwinkel** des Reifens. Der Übergang ist bei 10 m/s abgeschlossen ([`SetSteering` L1745–1765](https://github.com/VDrift/vdrift/blob/f835edaef1abe586b8d5bebeaa8c41f85e6dbea4/src/physics/cardynamics.cpp#L1745-L1765)). Das ist sehr wirksam für Tastatur, weil ein voller Tastendruck dann nie über den Grip-Peak hinaus lenkt.
  - VPP „Steering Limit: Automatically reduces the available steering range based on the speed of the vehicle“ ([VPP](https://vehiclephysics.com/advanced/misc-topics-explained/)).
- **Gegenlenk-Assistent:** VPP „Steering Help: Automatically moves the steering wheel to the direction that keeps the vehicle controlled under lateral sliding“ ([VPP](https://vehiclephysics.com/advanced/misc-topics-explained/)). Praktisch heißt das: Bei Driftwinkel β wird der Lenk-Nullpunkt um etwa β in Richtung Fahrtrichtung verschoben, sodass „Taste loslassen“ automatisch gegenlenkt **[Empfehlung, Detail unverifiziert]**.
- **Gas und Bremse per Tastatur:** Dieselbe Rise/Fall-Logik (z. B. 0 → 1 in 0.15–0.3 s) verhindert digitale Lastwechsel-Schläge **[Empfehlung]**.

---

## 8. Prior Art im Überblick

| Projekt | Was es auszeichnet | Primärquelle |
|---|---|---|
| **VDrift** (C++, Bullet) | Pac94-Reifen mit kombiniertem Schlupf; Reifen, Kupplung, Differenzial und Bremsen als **Constraints** in 10 Substeps; ABS/TCS/Steering Assist; hybride Replays | [github.com/VDrift/vdrift](https://github.com/VDrift/vdrift) (Code oben zitiert) |
| **Speed Dreams** (TORCS-Fork) | 500 Hz, eigene Starrkörperintegration, vereinfachte Magic Formula auf Schlupfbetrag, Lastempfindlichkeit, viele Diff-Typen, ABS/TCL/ESP „in simu“ | [forge.a-lec.org/speed-dreams](https://forge.a-lec.org/speed-dreams/speed-dreams-code); laut README „a fork of the TORCS project“ |
| **TORCS** | Vorfahr von Speed Dreams (simuv2) | [torcs.sourceforge.net](https://torcs.sourceforge.net/); Code nicht separat gelesen **[unverifiziert]** |
| **Rigs of Rods** | Soft-Body (Knoten und Balken) mit 2 kHz; für GT-Stil zu teuer und zu „weich“ | [`SimConstants.h`](https://github.com/RigsOfRods/rigs-of-rods/blob/d426a5c5e9f76a77a34e5fba8a41cda20edbe97f/source/main/physics/SimConstants.h#L20) |
| **Edy's Vehicle Physics Pro** | Schlupfgeschwindigkeits-Reifenkurven (Adherent/Peak/Limit), 1 kHz intern über Substeps, Fahrhilfen-Modi | [vehiclephysics.com](https://vehiclephysics.com/) |
| **Marco Monster, „Car Physics for Games“** (2003) | Didaktisch: Slip, Gewichtsverlagerung, Antrieb, Low-Speed-Problem | Original-Site offline; [Spiegel (asawicki.info)](https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html) |
| **Brian Beckman, „The Physics of Racing“** (1991) | Gewichtsverlagerung, Traction Budget/Reibkreis | Spiegel: [Teil 1](https://cygnus-supra.duckdns.org/Supra/Library/PhysicsOfRacing/physics_of_racing_01.html), [Teil 7](https://cygnus-supra.duckdns.org/Supra/Library/PhysicsOfRacing/physics_of_racing_07.html) (keine autoritative Originalquelle gefunden) |
| **Racer** (Ruud van Gaal) | Klassische Pacejka-Doku für Spiele | racer.nl offline **[nicht verfügbar]** |
| **Chaos Vehicles** (Epic) | Bewusst nicht genutzt, aber das Integrationsmuster (`TSimCallbackObject`) ist Vorbild | [API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Chaos/TSimCallbackObject) |
| **GT6** selbst | Polyphony veröffentlicht keine Modelldetails | **[unverifiziert]** |

---

## 9. Empfehlung für M1

**Leitidee:** Ein **engine-unabhängiger Sim-Kern** in reinem C++ (kein UObject, keine Engine-Header außer Mathe). Dazu ein **dünner UE-Adapter**, der Raycasts liefert und Kräfte auf den Chaos-Rigid-Body schreibt. Das erfüllt die Vorgaben aus DESIGN.md: Eingabe-Pakete, fester Zeitschritt, Trennung Sim/Darstellung, gleiche Physik für alle. Der Kern ist außerdem ohne Editor testbar.

### 9.1 Module und Schnittstellen

```
Source/GTOSim/            (UE-Modul, aber Kern ohne UObject-Abhängigkeit)
  Input/
    FVehicleInputPacket     { uint32 Tick; float Steer[-1..1]; float Throttle, Brake, Clutch, Handbrake [0..1];
                              int8 GearRequest; uint8 AidFlags; }      // gleiche Struktur für Spieler/KI/Replay/Ghost
    FInputShaper            // nur für menschliche Geräte: Rise/Fall-Rate, Speed-Sensitivity,
                            // Peak-Slip-Lenkbegrenzung, Gegenlenkhilfe; liest letzten FVehicleState (read-only)
  Vehicle/
    FVehicleParams          // reine Daten (aus DataAsset geladen): Masse, Trägheit, Geometrie, Reifen, Feder, Motor ...
    FVehicleState           // vollständiger, kopierbarer Zustand (Schnappschuss für Replay/Ghost/Tests)
    ITireModel              // Evaluate(SlipRatio, SlipAngle, Fz, Mu, Camber) -> {Fx, Fy, Mz}; PeakSlip(Fz)
      FMagicFormulaTire     // Pac94-artig, Lastempfindlichkeit, kombinierter Schlupf via G-Funktionen
    FSuspension             // Feder/Dämpfer/Bumpstop + Stabi pro Achse -> Fz
    FDrivetrain             // Motor-LUT, Kupplung, Getriebe, Diff (open, clutch-LSD), Raddrehträgheit
    FAero                   // Drag + Abtrieb pro Achse
    FDriverAids             // ABS/TCS/ESC: Packet + State -> Aktor-Kommandos (Bremsdruck je Rad, Gas)
    FVehicleSim             // Step(dt, Packet, ChassisKinematics, WheelContacts[4]) -> ChassisForces + neuer State
  Unreal/
    UGTOVehicleComponent    // Adapter: Contacts per Trace, ruft FVehicleSim, AddForceAtPosition / AddTorque
    AGTOVehiclePawn         // Darstellung: Mesh, Räder, Kamera (liest nur Snapshots, schreibt nie Sim-State)
    UGTOTelemetryWidget     // Debug-HUD: Slip, Fz, Kräfte je Rad, Motor, Gang
```

Schnittstellenregeln **[Empfehlung]**:

- `FVehicleSim::Step` ist eine **pure Funktion** von (State, Params, Packet, Contacts, dt). Unit-Tests laufen ohne Welt:
  - stationäre Achslasten gegen die Beckman-Formel
  - Ausrollen gegen den Drag
  - Peak-Kraft gegen die Magic Formula
  - Golden-Run-Determinismus: gleiche Pakete ergeben identischen State-Hash
- Die Darstellung liest nur `FVehicleState`-Snapshots, interpoliert zwischen den letzten beiden Sim-Ticks und schreibt nie zurück.
- Die KI (M2) erzeugt direkt `FVehicleInputPacket` und umgeht `FInputShaper`.

### 9.2 Zeitschritt und UE-Integration

- **Chaos:** `bTickPhysicsAsync = true`, `AsyncFixedTimeStepSize = 1/120 s` als Startwert, später ggf. 1/240 messen.
- **Fahrzeug-Inner-Loop:** Im `AsyncPhysicsTickComponent` des Adapters laufen **N = 8 interne Substeps** (also ca. 960 Hz). Das liegt im Bereich von VDrift (900 Hz) und VPP (1000 Hz).
- **Pro Chaos-Schritt:**
  1. Einmal pro Rad tracen (Shape-Cast). Kontakt (Punkt, Normale, Reibwert des Physical Material) für alle Substeps einfrieren.
  2. N Substeps: Federweg und Fz aus Chassis-Pose plus lokal mitintegrierter Chassis-Geschwindigkeit, Relaxations-Schlupf, Reifenkräfte, Antriebsstrang und Raddrehung integrieren.
  3. Die **gemittelten** Kräfte und Momente einmal auf den Chaos-Body geben (`AddForceAtPosition` über das Async-Handle).

  Dass das Chassis innerhalb der Substeps lokal mitintegriert wird, ist eine Näherung. Ob sie reicht, muss ein **Spike** zeigen. Die Alternative ist, wie VDrift Impulse direkt im Substep anzuwenden.
- **Warum `AsyncPhysicsTickComponent` für M1:** Es läuft laut Epic auf dem Game Thread mit Zugriff auf Physikdaten. Traces und UObject-Zugriffe sind damit einfach. Der dokumentierte Sync-Overhead ist bei 1 Auto (M1) bzw. 11 Autos (M2) voraussichtlich tragbar **[unverifiziert, messen]**. Wenn das Profiling dagegen spricht, auf `TSimCallbackObject` umziehen (Muster von Chaos Vehicles). Weil der Sim-Kern engine-unabhängig ist, betrifft der Umzug nur den Adapter.
- **Eingabe-Pakete:** Der Game Thread schreibt Pakete in eine Queue mit Tick-Nummer. Der Physik-Tick konsumiert genau ein Paket pro Chaos-Schritt; fehlt eines, wird das letzte wiederholt. Ein Recorder speichert Pakete und alle 60 Ticks einen `FVehicleState`. Das ist die Grundlage für die hybriden Replays/Ghosts in M2 (VDrift-Muster).
- **Vor dem Festpinnen der Engine-Version prüfen:**
  1. Auf welchem Thread läuft `AsyncPhysicsTickComponent` wirklich?
  2. Welches Minimum erlaubt `AsyncFixedTimeStepSize`?
  3. Tritt das gemeldete Slowdown-Problem bei niedrigen FPS auf?

### 9.3 Stufenplan innerhalb von M1 (entsprechend DESIGN.md)

| Stufe | Inhalt | Abnahme |
|---|---|---|
| **0 – Gerüst** | Pawn, Async-Tick, Shape-Cast-Räder, Feder/Dämpfer/Stabi, Telemetrie-HUD, Input-Pakete und Recorder | Auto steht ruhig ohne Zittern; Achslasten im Stand = statische Verteilung (Test) |
| **1 – Reifen + Fahrwerk** | `FMagicFormulaTire` (Start: B/C/D/E-Satz, D ≈ 1.0, Lastempfindlichkeit), kombinierter Schlupf, Relaxationslänge (σ ≈ 0.3–0.5 m) mit v-Untergrenze, glatte Nennersättigung (VXLOW ≈ 1–3 m/s); Antrieb vorerst als **direktes Radmoment** | Stillstand stabil; Lenkansprechen; Gewichtsverlagerung spürbar; Drift kontrollierbar |
| **2 – Antriebsstrang** | Motor-LUT + Trägheit + Schleppmoment, Kupplung (Haft/Gleit), 6-Gang mit Schaltzeit, Auto-Schaltung, offenes Diff → Lamellen-LSD; Bremsen als begrenzter Impuls Richtung ω=0 | Kein Schwingen bei Gangwechsel/Kupplung; Bremsen blockiert sauber |
| **3 – Aero** | Drag + Abtrieb pro Achse | Plausible Vmax und Ausrollkurve |
| **4 – Fahrhilfen + Glättung** | `FInputShaper` (Rise/Fall, Speed-Sensitivity, Peak-Slip-Lenkbegrenzung, Gegenlenkhilfe), `FDriverAids` (ABS prädiktiv, TCS mit Hysterese, ESC über Gierraten-Soll) | Tastatur-Runden machen Spaß, also die M1-Frage |

**Bewusst nicht in M1:** Reifentemperatur und -verschleiß, Sturz/Kinematik-Kurven, Rückstellmoment Mz (nur für Lenkrad-FFB relevant), Ground Effect, Constraint-Solver à la VDrift (nur falls die explizite Lösung bei 960 Hz instabil bleibt).

### 9.4 Größte Risiken

1. **Steifigkeit Rad/Antriebsstrang/Bremse** bei expliziter Integration. Gegenmittel in dieser Reihenfolge: Substeps erhöhen, Bremse/Kupplung als begrenzte Impulse, schließlich VDrift-artige Constraints.
2. **Stillstand/Low-Speed-Zittern.** Gegenmittel: Relaxationslänge mit v-Untergrenze und Nennersättigung; notfalls VPP-artige Haftreibung unter ca. 1 m/s.
3. **UE-Async-Physics ist experimentell.** Gegenmittel: Der Sim-Kern bleibt engine-unabhängig, der Adapter austauschbar.
4. **Determinismus von Chaos nicht zugesichert.** Gegenmittel: hybride Aufzeichnung (Eingaben + State-Keyframes); Ghosts nur aus States.
