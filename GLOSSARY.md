# ultragravity – Fahrphysik für APEXLINE

Ein eigenständiges Fahrphysik-Paket, das im Terminal entsteht und getestet wird und später in APEXLINE (Unreal) einzieht.

## Paket und Umgebung

**Physikpaket**:
Der Teil, der später nach APEXLINE übernommen wird: Fahrzeugmodell, Fahrhilfen, Eingabe-Glättung, Aufzeichnung, Telemetrie und PP-Wert.
_Avoid_: Sim-Kern, Engine, Library

**Testwelt**:
Der Ersatz für Unreal im Terminal. Sie bewegt die Karosserie und liefert dem Physikpaket die Radkontakte. Sie zieht nicht nach APEXLINE um.
_Avoid_: Mock, Simulator, Harness

**Testgelände**:
Die Strecke der Testwelt: eine unbegrenzte Ebene mit Belagzonen.
_Avoid_: Map, Level, Teststrecke

**Belagzone**:
Ein Bereich des Testgeländes mit eigenem Reibwert, etwa trocken oder nass.
_Avoid_: Material, Untergrund

**Eingabe-Paket**:
Die Steuerbefehle für einen Sim-Tick (Lenkung, Gas, Bremse, Kupplung, Handbremse, Gangwunsch, Fahrhilfen). Einziger Weg, ein Auto zu steuern, gleich für Spieler, KI, Replay und Ghost.
_Avoid_: Input, Controls, Kommando

## Fahrzeuge und Abstimmung

**Fahrzeugdatei**:
Alle Kennwerte eines Autos (Masse, Geometrie, Reifen, Fahrwerk, Antrieb, Aero), getrennt vom Code. Ein Auto ist genau eine Fahrzeugdatei.
_Avoid_: Config, Preset, Setup

**Sperrdifferenzial**:
Lamellen-Differenzial einer angetriebenen Achse mit Vorspannung und lastabhängiger Sperrwirkung. Ohne Sperre heißt es offenes Differenzial.
_Avoid_: Torsen, Limited Slip

**Automatik**:
Getriebemodus, in dem das Auto selbst kuppelt und schaltet. Sie rechnet mit der Drehzahl, die zur Geschwindigkeit über Grund gehört, und hält im Drift den Gang.
_Avoid_: Automatikgetriebe

**Abstimm-Werkzeug**:
Fährt Testmanöver mit einem Auto und vergleicht die Ergebnisse mit dessen Zielwerten.
_Avoid_: Tuner, Optimizer

**Testmanöver**:
Ein festgelegter Fahrversuch mit messbarem Ergebnis, etwa Beschleunigung 0–100 km/h oder Bremsweg 100–0 km/h.
_Avoid_: Benchmark, Szenario

**Testfahrer**:
Einfache Regler, die ein Testmanöver über Eingabe-Pakete fahren. Er ist kein Rennfahrer und bleibt in der Testwelt.
_Avoid_: KI, Bot, Autopilot

**Zielwert**:
Das gewünschte Ergebnis eines Testmanövers für ein bestimmtes Auto.
_Avoid_: Sollwert, Target

## Fahrverhalten

**Drift**:
Eine Fahrt jenseits der Haftgrenze mit ausbrechendem Heck, die der Fahrer mit Gegenlenken und Gas absichtlich hält.
_Avoid_: Schleudern, Slide

**Rutschen**:
Eine Fahrt jenseits der Haftgrenze, die niemand absichtlich hält. Daraus wird ein Drift oder ein Dreher, oder das Auto fängt sich.
_Avoid_: Schleudern, Wegschmieren

**Dreher**:
Das Heck überholt die Front, und das Auto dreht sich um die eigene Hochachse, ohne dass der Fahrer es noch abfangen kann.
_Avoid_: Spin, Turbulenz

**Driftwinkel**:
Der Winkel zwischen der Längsachse des Autos und seiner Fahrtrichtung.
_Avoid_: Schwimmwinkel, Slip-Winkel, Drift-Angle

## Ausgaben

**Animationsdaten**:
Der sichtbare Zustand eines Autos pro Tick: Raddrehung, Lenkwinkel, Federweg je Rad, Nicken und Wanken, Lenkrad, Drehzahl. Die Darstellung liest sie nur.
_Avoid_: Animation, Render-State

**Telemetrie**:
Messwerte zur Analyse des Fahrverhaltens, etwa Schlupf, Radlast und Kräfte je Rad, Motor und Gang.
_Avoid_: Debug-Daten, Log

**Aufzeichnung**:
Die gespeicherten Eingabe-Pakete und Zustände einer Fahrt, aus denen Replays und Ghosts entstehen.
_Avoid_: Recording, Replay-Datei

**PP-Wert**:
Eine Zahl für die Leistungsfähigkeit eines Autos, berechnet aus seinen Ergebnissen in den Testmanövern. Events setzen damit Obergrenzen.
_Avoid_: Rating, Klasse, Performance-Index

## Fahrerunterstützung

**Fahrhilfe**:
ABS, Traktionskontrolle oder Stabilitätskontrolle. Eine Fahrhilfe greift zwischen Eingabe-Paket und Bremsen oder Gas ein und lässt die Physik unverändert.
_Avoid_: Assist, Driver Aid

**Eingabe-Glättung**:
Macht digitale Eingaben von Tastatur und Gamepad weich und fahrbar, bevor sie zum Eingabe-Paket werden. Gilt nur für menschliche Fahrer.
_Avoid_: Input-Filter, Smoothing
