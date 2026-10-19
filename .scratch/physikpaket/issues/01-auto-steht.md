# 01: Auto steht auf dem Testgelände

**What to build:** Der erste durchgehende Weg von der Fahrzeugdatei bis zum grünen Test. Ein Auto wird aus seiner Fahrzeugdatei geladen, von der Testwelt über dem Testgelände abgesetzt, federt ein und steht dann ruhig. Dafür entsteht das Gerüst: CMake-Bau mit doctest, das Physikpaket als eigenes Bibliotheksziel (ohne Abhängigkeit zur Testwelt), der eingebundene JSON-Parser im Modus ohne Exceptions, die Schnittstelle des Fahrzeug-Schritts (siehe Spec, Naht 2), das Eingabe-Paket, die Testwelt mit starrem Körper (1/120 s) und Ebene sowie Feder, Dämpfer (Druck-/Zugstufe) und Anschlag pro Rad. Die erste Fahrzeugdatei ist das Sportcoupé mit Masse, Trägheit, Schwerpunkt, Radpositionen und Fahrwerkswerten. Dazu kommt die Prüfsumme über den Zustand. Einheiten und Achsen nach ADR-0004.

**Blocked by:** None (can start immediately)

**Status:** ready-for-agent

- [ ] Ein Befehl baut alles, ein Befehl führt alle Tests aus.
- [ ] Das Physikpaket baut als eigenes Ziel ohne Testwelt.
- [ ] Ein abgesetztes Auto federt ein und kommt zur Ruhe; danach bewegt es sich über mehrere Sekunden praktisch nicht (kein Zittern, kein Wegrutschen).
- [ ] Die Achslasten im Stand stimmen mit der statischen Verteilung aus Schwerpunktlage und Radstand überein (Toleranz).
- [ ] Zwei Läufe mit denselben Eingabe-Paketen ergeben dieselbe Prüfsumme (ADR-0005).
- [ ] Der Fahrzeug-Schritt ist eine reine Funktion ohne globalen Zustand.
