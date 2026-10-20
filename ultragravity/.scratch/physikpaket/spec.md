# Spec: ultragravity – Physikpaket für APEXLINE

Status: ready-for-agent

## Problem Statement

Für APEXLINE (`~/workspace/GTO`) ist das Fahrgefühl der Kern des Spiels: GT6-Niveau, eigenes Fahrzeugmodell statt Chaos Vehicles. Die Fahrphysik ist zugleich der riskanteste Teil. In Unreal lässt sie sich aber nur auf dem Windows-PC entwickeln, im Editor, mit langen Zyklen und schlecht automatisch prüfbar. Ich will die Physik deshalb vorher auf meinem Linux-Rechner bauen, komplett im Terminal, mit Tests, die beweisen, dass sie stimmt. Sie soll später ohne Umschreiben in APEXLINE einziehen. Außerdem brauche ich einen Weg, das Fahrverhalten jedes Autos gezielt einzustellen und messbar zu machen. Und ich brauche einen PP-Wert, der wirklich widerspiegelt, wie gut ein Auto fährt.

## Solution

Ein Physikpaket in C++20 ohne Unreal-Abhängigkeit (ADR-0001). Es enthält das Fahrzeugmodell (Reifen, Fahrwerk, Antriebsstrang, Aero), die Fahrhilfen, die Eingabe-Glättung, die Animationsdaten, die Telemetrie, die Aufzeichnung und den Ladecode für die Fahrzeugdatei. Im selben Repo, aber außerhalb des Physikpakets, liegen:

- die **Testwelt**: ersetzt Chaos, bewegt die Karosserie und stellt das Testgelände bereit (ADR-0002)
- der **Testfahrer**
- das **Abstimm-Werkzeug**: fährt die Testmanöver, vergleicht sie mit den Zielwerten und berechnet daraus den PP-Wert (ADR-0003)

Alles wird mit CMake gebaut und läuft samt Tests im Terminal. Das erste Auto ist ein fiktives Sportcoupé mit Heckantrieb. Später bindet APEXLINE das Repo als privates Git-Submodul ein und schreibt dort einen dünnen Unreal-Adapter.

## User Stories

### Gerüst und Testwelt

1. Als Entwickler will ich das ganze Projekt mit einem einzigen CMake-Aufruf im Terminal bauen, damit ich ohne Editor und ohne Windows arbeiten kann.
2. Als Entwickler will ich alle Tests mit einem Befehl laufen lassen und eine klare Zusammenfassung sehen, damit ich nach jeder Änderung sofort weiß, ob die Physik noch stimmt.
3. Als Entwickler will ich ein Auto in der Testwelt auf das Testgelände setzen, damit ich die Physik ohne Unreal ausprobieren kann.
4. Als Entwickler will ich der Testwelt eine Folge von Eingabe-Paketen geben und den Verlauf der Fahrt zurückbekommen, damit ich beliebige Fahrsituationen reproduzierbar nachstellen kann.
5. Als Entwickler will ich Belagzonen mit eigenem Reibwert auf dem Testgelände festlegen, damit ich Fahren auf nassem und trockenem Belag vergleichen kann.
6. Als Entwickler will ich, dass ein Auto im Stand ruhig steht, ohne zu zittern oder wegzurutschen, damit die Grundlage stabil ist.
7. Als Entwickler will ich, dass die Achslasten im Stand der statischen Gewichtsverteilung aus der Fahrzeugdatei entsprechen, damit ich weiß, dass Fahrwerk und Schwerpunkt stimmen.
8. Als Entwickler will ich, dass ein Auto nach dem Absetzen auf den Boden einfedert und sich beruhigt, damit Feder und Dämpfer plausibel arbeiten.

### Fahrzeugdatei

9. Als Abstimmer will ich alle Kennwerte eines Autos in einer Fahrzeugdatei im JSON-Format pflegen, damit ich das Fahrverhalten ändern kann, ohne Code anzufassen.
10. Als Abstimmer will ich bei einer fehlerhaften Fahrzeugdatei eine verständliche Fehlermeldung mit dem betroffenen Feld bekommen, damit ich Tippfehler schnell finde.
11. Als Abstimmer will ich, dass unplausible Werte (negative Masse, Reifen ohne Grip) beim Laden abgelehnt werden, damit kein kaputtes Auto in die Simulation gelangt.
12. Als Unreal-Adapter will ich dieselbe Fahrzeugdatei mit demselben Ladecode lesen, damit Terminal und Spiel garantiert dasselbe Auto fahren.
13. Als Unreal-Adapter will ich, dass der Ladecode ohne Exceptions auskommt, damit er in Unreal mit den Standard-Compilereinstellungen läuft.
14. Als Abstimmer will ich eine fertige Fahrzeugdatei für das erste Auto (Sportcoupé, Heckantrieb, rund 1350 kg, 300 PS, sechs Gänge, Sperrdifferenzial) mit Zielwerten haben, damit ich sofort etwas zum Fahren und Messen habe.

### Reifen und Fahrwerk

15. Als Entwickler will ich, dass die Reifenkraft der Magic Formula mit Lastempfindlichkeit folgt, damit Gewichtsverlagerung den Grip realistisch verändert.
16. Als Entwickler will ich kombinierten Schlupf, damit Bremsen in der Kurve Seitenführung kostet (Reibkreis).
17. Als Entwickler will ich, dass das Auto bei niedriger Geschwindigkeit und beim Anfahren nicht zittert, damit Stillstand und Rangieren stabil sind.
18. Als Entwickler will ich eine Relaxationslänge im Reifenmodell, damit sich die Seitenkraft nachvollziehbar aufbaut und das Auto bei hoher Geschwindigkeit nicht schwingt.
19. Als Entwickler will ich Stabilisatoren pro Achse, damit ich über die Rollsteifigkeitsverteilung Unter- und Übersteuern einstellen kann.
20. Als Entwickler will ich beim Bremsen Nicken und in der Kurve Wanken sehen, damit sich Gewichtsverlagerung von selbst aus der Bewegung der Karosserie ergibt.
21. Als Entwickler will ich, dass Räder beim starken Bremsen sauber blockieren, ohne um null zu schwingen, damit Bremsen ohne ABS realistisch ist.

### Antriebsstrang

22. Als Abstimmer will ich eine Drehmomentkurve des Motors festlegen, damit jedes Auto seinen eigenen Charakter hat.
23. Als Entwickler will ich Motorträgheit, Leerlauf und Motorbremse, damit Gaswegnehmen und Anfahren sich echt anfühlen.
24. Als Entwickler will ich eine Kupplung, die haftet oder rutscht, damit Anfahren und durchdrehende Räder möglich sind.
25. Als Abstimmer will ich Übersetzungen und Schaltzeit pro Auto festlegen, damit Beschleunigung und Vmax stimmen.
26. Als Spieler will ich zwischen manuellem und automatischem Getriebe wählen können, wie in GT6.
27. Als Entwickler will ich, dass Gangwechsel und Kupplungsvorgänge kein Schwingen im Antriebsstrang erzeugen, damit die Simulation stabil bleibt.
28. Als Abstimmer will ich zwischen offenem Differenzial und Lamellen-Sperre wählen und die Sperre einstellen, damit sich Traktion am Kurvenausgang abstimmen lässt.
29. Als Abstimmer will ich Front-, Heck- und Allradantrieb festlegen können, damit alle Autotypen für v1.0 möglich sind.

### Aero

30. Als Entwickler will ich Luftwiderstand, damit Autos eine plausible Höchstgeschwindigkeit und Ausrollkurve haben.
31. Als Abstimmer will ich Abtrieb pro Achse festlegen, damit die Aero-Balance das Fahrverhalten bei hoher Geschwindigkeit bestimmt.
32. Als Entwickler will ich Rollwiderstand pro Rad abhängig von der Radlast, damit Ausrollen realistisch ist.

### Testfahrer, Testmanöver und Abstimm-Werkzeug

33. Als Abstimmer will ich die sechs Testmanöver mit einem Befehl fahren lassen, damit ich ein Auto in Sekunden vermessen kann.
34. Als Abstimmer will ich das Testmanöver **Beschleunigung 0–100 km/h** mit der Zeit als Ergebnis.
35. Als Abstimmer will ich das Testmanöver **Beschleunigung 0–200 km/h** mit der Zeit als Ergebnis.
36. Als Abstimmer will ich das Testmanöver **Bremsweg 100–0 km/h**, einmal mit und einmal ohne ABS, mit der Strecke als Ergebnis.
37. Als Abstimmer will ich das Testmanöver **Kreisfahrt** auf 100 m Radius mit der maximalen Querbeschleunigung in g als Ergebnis.
38. Als Abstimmer will ich das Testmanöver **Höchstgeschwindigkeit** mit der Geschwindigkeit als Ergebnis.
39. Als Abstimmer will ich das Testmanöver **Spurwechsel** (Elchtest) mit der höchsten Einfahrgeschwindigkeit ohne Dreher als Ergebnis.
40. Als Abstimmer will ich für jedes Testmanöver den Zielwert, das Ergebnis und die Abweichung in einer übersichtlichen Tabelle im Terminal sehen, damit ich weiß, an welchen Kennwerten ich drehen muss.
41. Als Abstimmer will ich die Zielwerte eines Autos in seiner Fahrzeugdatei pflegen, damit Auto und Ziel zusammen bleiben.
42. Als Entwickler will ich, dass der Testfahrer ausschließlich über Eingabe-Pakete steuert, damit er keine Abkürzung an der Physik vorbei nimmt.
43. Als Abstimmer will ich, dass ein gescheitertes Testmanöver (Dreher, Ziel nicht erreicht) klar als gescheitert gemeldet wird statt mit einer falschen Zahl.

### Fahrhilfen

44. Als Spieler will ich ABS an- und ausschalten, damit ich beim Bremsen lenken kann, oder darauf verzichten.
45. Als Spieler will ich die Traktionskontrolle in Stufen 0–5 wählen, wie in GT6.
46. Als Spieler will ich die Stabilitätskontrolle an- und ausschalten.
47. Als Entwickler will ich, dass Fahrhilfen nur zwischen Eingabe-Paket und Bremsen bzw. Gas eingreifen und die Physik unverändert lassen, damit alle Autos gleich behandelt werden.
48. Als Abstimmer will ich am Testmanöver Bremsweg sehen, was ABS bewirkt, damit ich ABS sinnvoll abstimmen kann.
49. Als Entwickler will ich, dass die Traktionskontrolle beim Anfahren mit Vollgas durchdrehende Räder spürbar begrenzt, je höher die Stufe, desto stärker.
50. Als Entwickler will ich, dass die Stabilitätskontrolle ein übersteuerndes Auto im Spurwechsel einfängt.

### Eingabe-Glättung

51. Als Spieler mit Tastatur will ich, dass ein voller Tastendruck die Lenkung weich aufbaut statt schlagartig, damit das Auto fahrbar bleibt.
52. Als Spieler will ich, dass die Lenkung schneller zurückstellt, als sie aufbaut.
53. Als Spieler will ich, dass der Lenkausschlag mit steigender Geschwindigkeit abnimmt, damit ich bei hohem Tempo nicht über den Grip hinaus lenke.
54. Als Spieler will ich eine Gegenlenkhilfe, damit ein rutschendes Heck beim Loslassen der Taste automatisch abgefangen wird.
55. Als Spieler mit Tastatur will ich, dass Gas und Bremse weich ansteigen, damit es keine schlagartigen Lastwechsel gibt.
56. Als Entwickler will ich, dass die Eingabe-Glättung nur für menschliche Fahrer gilt und Testfahrer und spätere KI sie umgehen.

### Animationsdaten

57. Als Unreal-Adapter will ich pro Tick die Animationsdaten bekommen: Raddrehung, Lenkwinkel, Federweg je Rad, Nicken, Wanken, Lenkrad, Drehzahl.
58. Als Unreal-Adapter will ich zwischen den letzten beiden Ticks interpolieren können, damit das Bild bei jeder Bildrate flüssig läuft.
59. Als Entwickler will ich, dass die Darstellung die Animationsdaten nur liest und nie in den Zustand zurückschreibt.

### Telemetrie und Aufzeichnung

60. Als Abstimmer will ich die Telemetrie einer Fahrt als CSV mit einer Zeile pro Tick bekommen, damit ich Schlupf, Radlast, Kräfte, Motor und Gang analysieren und plotten kann.
61. Als Entwickler will ich eine Fahrt als Aufzeichnung speichern, mit allen Eingabe-Paketen und alle 60 Ticks einem vollständigen Zustand.
62. Als Entwickler will ich, dass eine Aufzeichnung nach dem Schreiben und Lesen genau gleich ist.
63. Als Entwickler will ich, dass eine Aufzeichnung eine Versionsnummer trägt und eine fremde oder zu neue Version verständlich abgelehnt wird.
64. Als Unreal-Adapter will ich aus einer Aufzeichnung einen Ghost erzeugen können, der nur gespeicherte Zustände abspielt.

### Determinismus

65. Als Entwickler will ich, dass dieselben Eingabe-Pakete im selben Programm bitgenau dieselbe Fahrt ergeben, damit Replays stimmen und Physik-Regressionen sofort auffallen (ADR-0005).
66. Als Entwickler will ich eine Prüfsumme über den Zustand, damit ein Test zwei Fahrten in einem Vergleich prüfen kann.

### PP-Wert

67. Als Abstimmer will ich, dass das Abstimm-Werkzeug aus den gewichteten Ergebnissen der Testmanöver einen PP-Wert berechnet (ADR-0003).
68. Als Abstimmer will ich, dass der PP-Wert in die Fahrzeugdatei geschrieben wird, damit APEXLINE nur die fertige Zahl liest.
69. Als Abstimmer will ich, dass ein schnelleres Auto immer mehr PP bekommt als ein in allen Testmanövern schlechteres, damit PP-Grenzen fair sind.
70. Als Abstimmer will ich sehen, wenn der gespeicherte PP-Wert nicht mehr zum aktuellen Auto passt, damit ich nach Physikänderungen neu rechne.

### Übergabe an APEXLINE

71. Als Unreal-Adapter will ich das Physikpaket über eine einzige schmale Schnittstelle ansprechen (Zustand, Eingabe-Paket, Radkontakte, Karosseriebewegung und Zeitschritt rein; Kräfte, neuer Zustand und Animationsdaten raus), damit der Adapter dünn bleibt.
72. Als Unreal-Adapter will ich, dass das Physikpaket ohne Testwelt, Testfahrer und Abstimm-Werkzeug baut, damit nur der Teil nach APEXLINE kommt, der dorthin gehört.
73. Als Entwickler will ich, dass das Repo später als privates Git-Submodul in APEXLINE eingebunden werden kann.

## Implementation Decisions

### Module

- **Physikpaket** (wandert nach APEXLINE). Eigenes CMake-Ziel, nur Standardbibliothek. Innere Bausteine:
  - **Fahrzeugmodell:** Reifen, Fahrwerk, Antriebsstrang, Aero, Bremsen
  - **Fahrhilfen**
  - **Eingabe-Glättung**
  - **Animationsdaten** samt Interpolation
  - **Telemetrie**
  - **Aufzeichnung**
  - **Ladecode der Fahrzeugdatei**

  Nach außen gibt es eine Hauptschnittstelle, den Fahrzeug-Schritt, und daneben die Einstiege aus Naht 3.
- **Testwelt** (bleibt im Repo): ein starrer Körper für die Karosserie, das Testgelände und die Abtastung der Radkontakte. Sie ruft den Fahrzeug-Schritt genau so auf, wie es später der Unreal-Adapter tut.
- **Testfahrer** (bleibt im Repo): ein einfacher Regler pro Testmanöver, der nur Eingabe-Pakete erzeugt.
- **Abstimm-Werkzeug** (bleibt im Repo): ein Kommandozeilenprogramm, das Testmanöver fährt, die Tabelle aus Zielwert, Ergebnis und Abweichung ausgibt und den PP-Wert berechnet und speichert. Die Parameter werden noch nicht automatisch verändert.

### Schnittstelle Fahrzeug-Schritt (Naht 2)

- **Eingang:** Zustand, Kennwerte des Fahrzeugs, Eingabe-Paket, Radkontakte (pro Rad: Kontaktpunkt, Normale, Reibwert der Belagzone oder „kein Kontakt“), Bewegung der Karosserie (Lage, Geschwindigkeit, Drehgeschwindigkeit) und Zeitschritt
- **Ausgang:** Kräfte und Momente auf die Karosserie (über den Schritt gemittelt), der neue Zustand und die Animationsdaten
- **Reinheit:** Der Schritt ist eine reine Funktion. Er hat keinen globalen Zustand und keine Abhängigkeit von Uhrzeit oder Zufall.
- **Zwischenschritte:** Innerhalb eines Aufrufs rechnet das Paket 8 Zwischenschritte für Räder und Antriebsstrang (ADR-0006). Die Testwelt ruft den Schritt mit 120 Hz auf.
- **Zustand:** Er ist vollständig und kopierbar: Karosserie-Kinematik soweit nötig, Raddrehzahlen, Relaxationszustände, Motordrehzahl, Gang, Kupplung, Schaltzustand und die internen Zustände der Fahrhilfen.

### Eingabe-Paket

- **Felder:** Tick-Nummer, Lenkung (−1…1), Gas, Bremse, Kupplung und Handbremse (je 0…1), Gangwunsch und die Einstellungen der Fahrhilfen
- **Für wen:** Dieselbe Struktur gilt für Spieler, Testfahrer, spätere KI, Replay und Ghost.

### Einheiten und Achsen

- SI-Einheiten in `double`, Fahrzeugachsen nach ISO 8855 (X vorn, Y links, Z oben)
- Die Umrechnung ins Unreal-System macht erst der Adapter in APEXLINE (ADR-0004).

### Fahrzeugdatei

- **Format:** JSON, gelesen über einen eingebundenen Parser aus einer einzelnen Header-Datei, im Modus ohne Exceptions
- **Fehler:** Fehler kommen als Rückgabewert mit dem Pfad des betroffenen Felds zurück.
- **Inhalt:**
  - Masse, Trägheit, Schwerpunkt und Radpositionen
  - Reifen-Koeffizienten
  - Feder, Dämpfer (Druck- und Zugstufe), Anschlag und Stabilisatoren
  - Motorkurve, Trägheit und Schleppmoment, Kupplung, Übersetzungen, Schaltzeit
  - Antriebsart und Differenzial
  - Bremsen und Bremsbalance
  - Luftwiderstand und Abtrieb pro Achse
  - Zielwerte, PP-Wert und eine Kennung des Physikstands, gegen den der PP-Wert berechnet wurde (für Story 70)

### Physik-Ansatz

Grundlage ist `~/workspace/GTO/research/fahrphysik.md`, Abschnitt 9.

- **Reifen:**
  - Magic Formula mit lastabhängigen Koeffizienten (Pac94-artig)
  - kombinierter Schlupf über Gewichtungsfunktionen
  - Relaxationslänge mit einer Untergrenze für die Geschwindigkeit
  - geglättete Sättigung des Schlupfnenners bei niedriger Geschwindigkeit
- **Radlast:** Sie ergibt sich aus Feder, Dämpfer und Stabilisator und wird nach unten auf null begrenzt. Die Gewichtsverlagerung entsteht dadurch von selbst.
- **Bremsen und Kupplung:** werden als begrenzte Impulse Richtung Zielgeschwindigkeit umgesetzt.
- **Sperrdifferenzial:** Lamellen-Sperre mit Vorspannung und lastabhängiger Sperrwirkung
- **Fahrhilfen:**
  - ABS: prädiktiv, regelt auf den Peak-Schlupf
  - Traktionskontrolle: mit Hysterese, die Stufe bestimmt den Ziel-Schlupf
  - Stabilitätskontrolle: vergleicht Soll- und Ist-Gierrate und bremst einzelne Räder
- **Eingabe-Glättung:**
  - getrennte Raten für Auf- und Abbau
  - geschwindigkeitsabhängige Lenkbegrenzung bis zum Peak-Schräglaufwinkel
  - Gegenlenkhilfe über den Driftwinkel

### Testwelt

- **Karosserie:** ein starrer Körper mit sechs Freiheitsgraden und fester Schrittweite von 1/120 s
- **Testgelände:** eine unbegrenzte Ebene mit rechteckigen Belagzonen
- **Radkontakte:** werden einmal pro Tick und Rad abgetastet und gelten für alle Zwischenschritte dieses Ticks, so wie es später in Unreal geplant ist.

### Aufzeichnung und Telemetrie

- **Aufzeichnung:** ein eigenes Binärformat mit Kennung und Versionsnummer. Es enthält alle Eingabe-Pakete und alle 60 Ticks einen vollständigen Zustand.
- **Telemetrie:** CSV mit Kopfzeile und einer Zeile pro Tick.

### PP-Wert

- Er wird aus den Ergebnissen der Testmanöver berechnet: Beschleunigung, Querbeschleunigung, Bremsweg und Vmax, gewichtet zu einer Zahl.
- Die Gewichte stehen an einer Stelle im Abstimm-Werkzeug.
- Eine Bedingung gilt immer: Ein Auto, das in allen Testmanövern besser ist, bekommt mehr PP.

### Bauen

- CMake mit Ninja, C++20, doctest als eingebundene Header-Datei
- Das Physikpaket ist ein eigenes Bibliotheksziel ohne Abhängigkeit zur Testwelt.
- Die Werkzeuge liegen unter `~/.local/bin`, in einer eigenen Python-Umgebung.

### Reihenfolge der Meilensteine

1. **Gerüst:** Testwelt, Fahrzeugdatei, Eingabe-Paket, Fahrwerk, Reifen mit direktem Radmoment und Animationsdaten. Abnahme: Das Auto steht ruhig, die Achslasten stimmen, der Bremsweg-Test läuft.
2. Antriebsstrang
3. Aero
4. Testfahrer und Abstimm-Werkzeug mit allen sechs Testmanövern
5. Fahrhilfen und Eingabe-Glättung
6. Telemetrie und Aufzeichnung
7. PP-Wert

## Testing Decisions

### Was ein guter Test ist

Ein guter Test prüft nur äußeres Verhalten an einer der drei Nähte, nie die inneren Bausteine. Reifen, Fahrwerk und Antriebsstrang werden nicht einzeln getestet: Ihr Verhalten zeigt sich an der Fahrt, und sie sollen frei umbaubar bleiben. Physikalische Erwartungen kommen aus unabhängigen Rechnungen:

- Achslasten nach Beckman
- Bremsweg aus μ·g
- Kreisfahrt aus v²/r
- Vmax aus dem Gleichgewicht von Antriebskraft und Luftwiderstand

Sie werden mit sinnvollen Toleranzen geprüft, nicht mit exakten Zahlen.

### Naht 1 – Testwelt-Lauf (der Großteil der Tests)

- Stillstand ohne Zittern
- statische Achslasten
- Einfedern und Beruhigen
- Bremsweg trocken und nass
- Nicken und Wanken in die richtige Richtung
- Räder blockieren ohne Schwingen
- 0–100 und 0–200
- Gangwechsel ohne Schwingen
- Vmax und Ausrollen
- Kreisfahrt-Querbeschleunigung
- Spurwechsel
- Wirkung jeder Fahrhilfe
- Determinismus über die Prüfsumme
- PP-Reihenfolge: das bessere Auto bekommt mehr PP

### Naht 2 – Fahrzeug-Schritt

Wenige Vertragstests:
- Ohne Kontakt gibt es keine Reifenkraft.
- Der Schritt verändert keinen fremden Zustand.
- Gleicher Eingang ergibt gleichen Ausgang.
- Die Animationsdaten passen zum Zustand.

### Naht 3 – die anderen Einstiege

- Fahrzeugdatei laden: gültig, ungültig mit Feldpfad, unplausible Werte
- Aufzeichnung: hin und zurück identisch, fremde Version wird abgelehnt
- Eingabe-Glättung: Auf- und Abbau, Lenkbegrenzung, Gegenlenkhilfe
- Interpolation der Animationsdaten

### Vorbilder

Im Repo gibt es noch keine Tests. Fachliches Vorbild sind die in `~/workspace/GTO/research/fahrphysik.md` genannten Abnahmekriterien (Abschnitt 9.3) und die Golden-Run-Idee aus Abschnitt 5.3.

## Out of Scope

- KI-Rennfahrer und Ideallinie
- Live-Ansicht oder ASCII-Animation im Terminal
- Unreal-Adapter, Pawn, Kamera, HUD: entstehen in APEXLINE
- Reifentemperatur und Reifenverschleiß, Sturz- und Kinematikkurven, Rückstellmoment
- Steigungen, Kuppen, Randsteine und Strecken mit Kurvenverlauf im Testgelände
- Automatisches Abstimmen der Parameter durch das Abstimm-Werkzeug
- Bitgleichheit zwischen Linux und Windows
- Constraint-Löser à la VDrift (nur falls die explizite Rechnung instabil bleibt, ADR-0006)
- Schaden, Wetter, Lenkrad und Force Feedback, Multiplayer

## Further Notes

- Vor dem Einzug in APEXLINE muss das Repo als privates GitHub-Repo angelegt werden.
- Die offenen Unreal-Fragen aus `~/workspace/GTO/research/fahrphysik.md`, Abschnitt 9.2 betreffen nur den Adapter, nicht dieses Paket. Dazu gehören der Thread von `AsyncPhysicsTickComponent` und der kleinste mögliche Zeitschritt.
- Die Zielwerte des ersten Autos (0–100 in 5,0 s, Bremsweg 35 m, 1,0 g, Vmax 265 km/h) sind Startwerte, keine Abnahmekriterien.
- Begriffe: siehe `GLOSSARY.md`. Entscheidungen: `docs/adr/0001` bis `0006`.
