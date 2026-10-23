# SORPASSO

Ein Rennspiel in Unreal Engine 5 mit eigener Fahrphysik. Kanonisch ist der englische Begriff, wie er im Code steht; in Klammern das deutsche Synonym, mit dem wir darüber reden.

## Paket und Umgebung

**Physics package** (Physikpaket):
Die Fahrphysik des Spiels: Fahrzeugmodell, Driving aids, Input smoothing, Recording, Telemetry und PP value. Sie kennt Unreal nicht.
_Avoid_: Sim-Kern, Engine, Library, ultragravity (nur Name des Namensraums und Ordners)

**Adapter** (Unreal-Adapter):
Die Naht zwischen Unreal und dem Physics package. Er liefert dem Paket Wheel contacts und Input packets und gibt dessen Kräfte an Chaos weiter.
_Avoid_: Bridge, Wrapper

**Test world** (Testwelt):
Der Ersatz für Unreal im Terminal. Sie bewegt die Karosserie und liefert dem Physics package die Wheel contacts. Sie gehört nicht zum Spiel.
_Avoid_: Mock, Simulator, Harness

**Test ground** (Testgelände):
Die Strecke der Test world: eine unbegrenzte Ebene mit Surface zones.
_Avoid_: Map, Level, Teststrecke

**Test track** (Teststrecke):
Die Strecke in Unreal, auf der man mit Rundenzeit fährt.
_Avoid_: Testgelände, Rennstrecke

**Surface zone** (Belagzone):
Ein Bereich des Test ground mit eigenem Reibwert, etwa trocken oder nass.
_Avoid_: Material, Untergrund

**Wheel contact** (Radkontakt):
Ob ein Rad den Boden berührt, und wenn ja, wo, mit welcher Normalen und welchem Reibwert.
_Avoid_: Hit, Raycast-Ergebnis

**Probe ray** (Abtaststrahl):
Die Strecke, entlang der die Umgebung pro Rad nach dem Boden sucht, um den Wheel contact zu bestimmen.
_Avoid_: Raycast, Trace

**Input packet** (Eingabe-Paket):
Die Steuerbefehle für einen Sim-Tick (Lenkung, Gas, Bremse, Kupplung, Handbremse, Gangwunsch, Driving aids). Einziger Weg, ein Auto zu steuern, gleich für Spieler, KI, Replay und Ghost.
_Avoid_: Input, Controls, Kommando

## Fahrzeuge und Abstimmung

**Vehicle file** (Fahrzeugdatei):
Alle Vehicle params eines Autos, getrennt vom Code. Ein Auto ist genau eine Vehicle file.
_Avoid_: Config, Preset, Setup, Datenobjekt

**Vehicle params** (Kennwerte):
Die Zahlen, die ein Auto physikalisch beschreiben: Masse, Geometrie, Reifen, Fahrwerk, Antrieb, Aero.
_Avoid_: Parameter, Tuning-Werte

**Limited-slip differential** (Sperrdifferenzial):
Lamellen-Differenzial einer angetriebenen Achse mit Vorspannung und lastabhängiger Sperrwirkung. Ohne Sperre heißt es **open differential** (offenes Differenzial).
_Avoid_: Torsen, Sperre

**Reverse gear** (Rückwärtsgang):
Gang −1: Der Motor dreht die Räder rückwärts. Er legt sich nur ein, solange das Auto (fast) steht; aus der Vorwärtsfahrt bleibt es im Leerlauf. Die Automatic kennt nur Rückwärts, Leerlauf und Fahren.
_Avoid_: R-Gang, Gang 0

**reverse_ratio** (Rückwärtsübersetzung):
Das Feld der Vehicle file mit der Übersetzung des Reverse gear, positiv angegeben wie die Vorwärtsgänge; die Achsübersetzung kommt noch dazu. Pflichtfeld.
_Avoid_: Rückwärtsgang-Faktor

**Automatic** (Automatik):
Getriebemodus, in dem das Auto selbst kuppelt und schaltet. Im Drift hält es den Gang.
_Avoid_: Automatikgetriebe

**Tuning tool** (Abstimm-Werkzeug):
Fährt Test maneuvers mit einem Auto und vergleicht die Ergebnisse mit dessen Target values.
_Avoid_: Tuner, Optimizer

**Test maneuver** (Testmanöver):
Ein festgelegter Fahrversuch mit messbarem Ergebnis, etwa Beschleunigung 0–100 km/h oder Bremsweg 100–0 km/h.
_Avoid_: Benchmark, Szenario

**Test driver** (Testfahrer):
Einfache Regler, die ein Test maneuver über Input packets fahren. Er ist kein Rennfahrer und bleibt in der Test world.
_Avoid_: KI, Bot, Autopilot

**Target value** (Zielwert):
Das gewünschte Ergebnis eines Test maneuver für ein bestimmtes Auto.
_Avoid_: Sollwert, Target

## Fahrverhalten

**Drift**:
Eine Fahrt jenseits der Haftgrenze mit ausbrechendem Heck, die der Fahrer mit Gegenlenken und Gas absichtlich hält.
_Avoid_: Schleudern

**Slide** (Rutschen):
Eine Fahrt jenseits der Haftgrenze, die niemand absichtlich hält. Daraus wird ein Drift oder ein Spin, oder das Auto fängt sich.
_Avoid_: Schleudern, Wegschmieren

**Spin** (Dreher):
Das Heck überholt die Front, und das Auto dreht sich um die eigene Hochachse, ohne dass der Fahrer es noch abfangen kann.
_Avoid_: Turbulenz

**Drift angle** (Driftwinkel):
Der Winkel zwischen der Längsachse des Autos und seiner Fahrtrichtung.
_Avoid_: Schwimmwinkel, Slip angle

**Slip angle** (Schräglaufwinkel):
Der Winkel zwischen der Richtung, in die ein Rad zeigt, und der, in die es sich über den Boden bewegt.
_Avoid_: Drift angle, Schlupfwinkel

**Relaxation length** (Relaxationslänge):
Die Rollstrecke, über die sich der Slip angle eines Reifens an eine neue Fahrtrichtung angleicht.
_Avoid_: Reifenverzögerung

## Ausgaben

**Animation data** (Animationsdaten):
Der sichtbare Zustand eines Autos pro Tick: Raddrehung, Lenkwinkel, Federweg je Rad, Nicken und Wanken, Lenkrad, Drehzahl. Die Darstellung liest sie nur.
_Avoid_: Animation, Render-State

**Telemetry** (Telemetrie):
Messwerte zur Analyse des Fahrverhaltens, etwa Schlupf, Radlast und Kräfte je Rad, Motor und Gang.
_Avoid_: Debug-Daten, Log

**Recording** (Aufzeichnung):
Die gespeicherten Input packets und Zustände einer Fahrt, aus denen Replays und Ghosts entstehen: jedes Input packet und alle 60 Ticks ein **Snapshot** (vollständiger Zustand), in einem eigenen Binärformat mit Kennung und Versionsnummer.
_Avoid_: Replay-Datei

**Replay**:
Die Fahrt aus einer Recording neu simuliert, aus ihren Input packets; im selben Programm bitgenau gleich.
_Avoid_: Wiedergabe

**Ghost**:
Ein Auto, das nur die gespeicherten Snapshots einer Recording wiedergibt (dazwischen interpoliert), ohne zu simulieren.
_Avoid_: Geisterauto, Schatten

**PP value** (PP-Wert):
Eine Zahl für die Leistungsfähigkeit eines Autos, berechnet aus seinen Ergebnissen in den Test maneuvers. Events setzen damit Obergrenzen. Das Tuning tool schreibt sie mit der Physics version in die Vehicle file (Block `pp`).
_Avoid_: Rating, Klasse, Performance-Index

**Physics version** (Physikstand):
Kennung, unter der ein PP value gerechnet wurde: die Prüfsumme einer festen Referenzfahrt mit dem Auto. Ändert sich die Physik oder das Auto, passt der gespeicherte PP value nicht mehr, und das Tuning tool warnt.
_Avoid_: Version, Build

## Fahrerunterstützung

**Driving aid** (Fahrhilfe):
ABS, Traktionskontrolle oder Stabilitätskontrolle. Eine Driving aid greift zwischen Input packet und Bremsen oder Gas ein und lässt die Physik unverändert.
_Avoid_: Assist, Driver Aid

**Input smoothing** (Eingabe-Glättung):
Macht digitale Eingaben von Tastatur und Gamepad weich und fahrbar, bevor sie zum Input packet werden. Gilt nur für menschliche Fahrer.
_Avoid_: Input-Filter
