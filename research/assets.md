# Freie Assets für APEXLINE: Lizenzen, Kandidaten, Fallstricke

> Stand: 2026-10-11. Recherche gegen Primärquellen (Lizenztexte, Asset-Seiten, Plattform-APIs).
> Vorgabe aus `DESIGN.md`: nur **kostenlos**, nur **CC0, CC-BY oder Fab Standard License**; kein NC, kein ND, kein „Editorial“. Fiktive Autos ohne echte Marken.
> **Keine Rechtsberatung.** Alles, was nicht direkt am Primärtext geprüft werden konnte, ist mit **[UNVERIFIZIERT]** markiert.
> Es wurden **keine Assets heruntergeladen**. Qualitätsangaben stammen aus Seitenbeschreibungen und API-Metadaten (Polygonzahl usw.), nicht aus eigener Sichtung. Sounds wurden nicht angehört.

---

## 0. Kurzfassung

| Quelle | Lizenz | Im Spiel ausliefern? | Namensnennung | Bewertung für APEXLINE |
|---|---|---|---|---|
| **Fab, Standard License** (auch kostenlose Listings, Megascans) | Fab EULA („Epic Marketplace Content License Agreement“) | Ja, als Teil des Projekts, nicht einzeln | nicht nötig | ✅ Hauptquelle für Umgebung/Megascans. Aufpassen bei „UE-Only“ (für uns egal, wir nutzen UE) |
| **Fab, CC-BY-Listings** | CC BY 4.0 | Ja | ja | ✅ |
| **Poly Haven** | CC0 | Ja | nein | ✅ beste Quelle für Texturen, HDRIs, Vegetation |
| **ambientCG** | CC0 | Ja (ausdrücklich auch für Spiele) | nein | ✅ Asphalt-/Straßen-/Boden-Materialien |
| **Sketchfab CC0 / CC BY** | CC0 / CC BY 4.0 | Ja | bei BY ja | ⚠️ nur mit Einzelprüfung, viele Rips mit falscher Lizenz |
| Sketchfab CC BY-SA | CC BY-SA 4.0 | – | – | ❌ verträgt sich laut Fab EULA nicht mit Fab-Content |
| Sketchfab BY-NC / BY-ND / Editorial | – | – | – | ❌ ausgeschlossen |
| **Freesound CC0 / CC BY** | CC0 / CC BY 3.0/4.0 | Ja | bei BY ja | ✅ für Motor/Reifen, aber wenig saubere Drehzahl-Loops |
| Freesound CC BY-NC / Sampling+ | – | – | – | ❌ |
| **UE-Templates/Samples (Vehicle Template)** | UE EULA („Examples“) | Ja, im UE-Produkt | nein | ✅ für M1 als Platzhalter |
| **City Sample Vehicles** (Fab) | Fab, „UE-Only Content“ | Ja, nur in UE-Produkten | nein | ✅ fiktive Stadtautos, gute Basis für KI-Feld und Tests |
| Sonniss GDC Bundle | eigene Lizenz | Ja | nein | ⚠️ nicht CC0/CC-BY/Fab, also nach DESIGN.md außen vor (müsste bewusst freigegeben werden) |

**Kernaussagen:**
1. **Hochwertige, fiktive Autos mit Interieur unter CC0/CC-BY sind selten.** Realistisch gibt es nur eine Handvoll brauchbarer Basismodelle. Für „wenige, sehr schöne Autos“ werden die meisten Modelle **überarbeitet oder selbst modelliert** werden müssen. Die Kandidatenliste in §2.1 taugt als Ausgangsmaterial.
2. **Umgebung ist leicht:** Poly Haven, ambientCG und kostenlose Fab-/Megascans-Inhalte decken Asphalt, Gras, Fels, Bäume und HDRIs fotorealistisch ab.
3. **Motorsound:** Auf Freesound gibt es wenige saubere Konstant-Drehzahl-Loops unter Last. Es gibt brauchbare Sets mit Leerlauf/Niedrig/Mittel/Hoch und viele Drehzahl-Sweeps. Alternative: Loops mit dem MIT-lizenzierten **engine-sim** selbst erzeugen (§2.3).
4. **Logos entfernen reicht nicht immer.** Die Karosserieform eines echten Autos kann als Trade Dress (USA) oder Design (EU) geschützt sein (§3.2).

---

## 1. Lizenzfakten (Primärquellen)

### 1.1 Fab Standard License (Fab EULA)

Die Fab-Seiten blockieren automatisierte Abrufe (HTTP 403). Geprüft wurden:
- die **Kurzfassung** auf [fab.com/eula](https://www.fab.com/eula), gelesen über den Wayback-Snapshot vom 2026-10-08 ([Snapshot](http://web.archive.org/web/20261008064631/https://www.fab.com/eula)),
- der **Volltext** „Epic Marketplace Content License Agreement“ als PDF auf Epics CDN ([PDF v4 r1](https://cdn1.epicgames.com/eulatracking-download/emporium/en/v4/r1/2b42ccdae5f8a8b42f1d7e7f869e4c9c.pdf)). Dass dieses PDF die *aktuell* gültige Fassung ist, ist **[UNVERIFIZIERT]**. Unter diesem Titel verlinkt Fab den Vertrag auf [fab.com/cla](https://www.fab.com/cla).

**Kurzfassung auf fab.com/eula** (nicht bindend, laut eigener Aussage):
- Erlaubt: „Use the assets commercially or privately“, „Modify and adjust the assets in order to incorporate them into your Projects“, „Commercially distribute your Projects with the Fab assets incorporated into it“, „Use the assets with any compatible tools (usage is not limited to Unreal Engine)“, Teilen mit Mitarbeitenden am Projekt ([Snapshot](http://web.archive.org/web/20261008064631/https://www.fab.com/eula)).
- Verboten: „Resell or redistribute the asset for free on a standalone basis“.
- „Both pricing tiers (Personal and Professional) grant you the same scope of rights.“ Und: „You are not required to give credit to the publisher.“

**Volltext (PDF), die für uns wichtigen Punkte** ([PDF](https://cdn1.epicgames.com/eulatracking-download/emporium/en/v4/r1/2b42ccdae5f8a8b42f1d7e7f869e4c9c.pdf)):
- **Stufen:** Personal (bzw. Personal – Reference Only) nur bei höchstens 100.000 USD Bruttoumsatz in den letzten 12 Monaten, sonst Professional. Gemessen wird zum Zeitpunkt der Transaktion. Bei kostenlosen Assets spielt das praktisch keine Rolle. Die Schwelle steht auch in [Epics Fab-Doku](https://dev.epicgames.com/documentation/en-us/fab/licenses-and-pricing-in-fab).
- **„Alternative License“:** Ist ein Listing als Open-Source-/CC-Lizenz markiert, gilt *diese* Lizenz und **nicht** die Standard License. Fab bietet als Alternative nur **CC BY 4.0** an ([Fab-Doku](https://dev.epicgames.com/documentation/en-us/fab/licenses-and-pricing-in-fab)).
- **Projekte verbreiten:** „you may Distribute a Project that incorporates Content as an included dependency to end users … only … as incorporated in the Project in object code and you must restrict end users from extracting or otherwise using Content outside of the Project.“ Ausdrücklich erlaubt: „you may Distribute software applications (such as video games) that include Content to the general public“.
- **Keine Einzelweitergabe:** Content darf nur an Mitarbeitende (z. B. „in a private online repository“) gehen. ⇒ **Fab-Assets nie in ein öffentliches Git-Repo.** Das private GitHub-Repo aus DESIGN.md passt.
- **„Non-Compatible Licenses“:** Fab-Content darf nicht mit Material kombiniert werden, dessen Lizenz verlangt, dass der Fab-Content unter andere Bedingungen fällt. Genannt werden ausdrücklich **GPL, LGPL** (außer bei dynamischem Linken) **und CC BY-SA**. ⇒ **Kein CC-BY-SA-Asset und kein GPL-Content** (z. B. Autos aus Speed Dreams, VDrift oder TORCS) im selben Projekt. **[UNVERIFIZIERT]**: Lizenzen dieser Projekte nicht einzeln geprüft, nur als Warnbeispiel genannt.
- **Weitere Verbote:** Content nicht „on a stand-alone basis“ verkaufen (das Projekt muss einen eigenen Mehrwert haben). Dritten nicht erlauben, Content in eigene Produkte zu übernehmen, „e.g. … world- or level-editing tools … that allow works to be exported“. ⇒ **Ein späteres Mod-/Strecken-Editor-Tool darf keine Fab-Assets exportierbar machen.** Herstellervermerke nicht entfernen. „NoAI Content“ nicht für generative KI verwenden. **„UE-Only Content“ nur mit Unreal Engine/UEFN** verwenden.
- **KI-Kennzeichnung:** Fab verlangt von Anbietern das Tag „CreatedWithAI“, wenn „a material portion of the Content is generated with Generative AI Programs“.

**Megascans / Quixel:**
- Bis 31.12.2024 war die ganze Fab-Megascans-Bibliothek gratis unter der Fab Standard License zu holen. Seit 2025 ist der Großteil kostenpflichtig, „though we will still offer some assets for free“. So der Beitrag „Access legacy Megascans on Bridge and Quixel.com“ im [Unreal-Engine-Forum](https://forums.unrealengine.com/t/access-legacy-megascans-on-bridge-and-quixel-com/2142664). Dass der Verfasser Epic-Mitarbeiter ist, ist **[UNVERIFIZIERT]**.
- Assets, die über Quixel.com/Bridge geholt wurden (Legacy-Bibliothek), fallen unter die **Epic Content License**. Neu holen ging dort nur bis 31.12.2024 ([Forum](https://forums.unrealengine.com/t/access-legacy-megascans-on-bridge-and-quixel-com/2142664)).
- Epic-Zitat laut [CG Channel](https://www.cgchannel.com/?p=161277): „When you acquire Quixel content on Fab – whether free or paid – you can use it forever.“ Der Epic-Blog selbst lieferte 403, daher ist das nur sekundär belegt **[UNVERIFIZIERT am Primärtext]**.
- Laut [CG Channel, Mai 2025](https://www.cgchannel.com/2025/05/get-260-free-3d-scanned-assets-for-creating-a-derelict-interior-in-ue5/) gibt es weiterhin dauerhaft freie Megascans-Pakete (z. B. „Derelict Corridor“) und alle zwei Wochen wechselnde Gratis-Assets auf Fab. Welche Megascans *heute* gratis sind, konnte nicht automatisiert geprüft werden (Fab blockiert) **[UNVERIFIZIERT]**. **Vorgehen:** Auf fab.com mit Filter „Free“ und Publisher „Quixel“ suchen und jedes Listing auf dem Lizenz-Reiter prüfen.

### 1.2 Epic-Inhalte: Engine-Templates, Samples, Epic Content License

- **UE EULA:** „Licensed Technology“ umfasst Engine Code, **„Examples“** („content made available by us in the Samples and Templates folders in the install directory“) und **„Starter Content“**. Diese dürfen in Produkten verbreitet werden ([UE EULA PDF auf Epics CDN](https://cdn1.epicgames.com/eulatracking-download/unreal_engine/en/v2/r1/7be11df29cd87f4c3fe0032ad5bc3220.pdf)). Ob das PDF v2 die aktuelle Fassung ist: **[UNVERIFIZIERT]**, maßgeblich ist [unrealengine.com/eula/unreal](https://www.unrealengine.com/eula/unreal).
  - Also darf das **Vehicle Template** (Varianten Timetrial mit „SportsCar“ und Offroad mit „OffroadCar“, laut [Epic-Doku](https://dev.epicgames.com/documentation/unreal-engine/vehicle-template-in-unreal-engine)) in APEXLINE stecken. Als Platzhalter für **M1** ist es ideal.
  - Royalty: 5 % erst ab 1 Mio. USD Lebenszeit-Bruttoumsatz pro Produkt (gleiches PDF). Für ein Gratis-Spiel irrelevant.
- **Was nicht über den Launcher installiert wird** (Marketplace/Fab-Content), fällt **nicht** unter die UE EULA, sondern unter die Content-Lizenz (gleiches PDF, §1).
- **City Sample Vehicles** (Fab, kostenlos, von Epic): 13 fahrbare Fahrzeuge (Limousine, Coupé, SUV, Schrägheck, Sportwagen, Pickup, Sattelzug, Müllwagen, Taxi, Bus, Lieferwagen). Kennzeichnung **„UE-Only Content – Licensed for Use Only with Unreal Engine-based Products“**, Format nur Unreal Engine, „Allows usage with AI: No“ ([Fab-Listing, Wayback-Snapshot 2025-11-13](http://web.archive.org/web/20251113193215/https://www.fab.com/listings/2909157b-ddfa-4cef-a925-69dc2467021f)). „UE-Only“ ist laut Fab EULA eine Einschränkung *innerhalb* der Standard License. Für ein UE5-Spiel ist das also okay.
  - **[UNVERIFIZIERT]**: Ob die Fahrzeuge Innenräume haben, die fürs Cockpit taugen, und wie nah ihr Design an echten Modellen liegt. Die Beschreibung sagt nur, die Fahrphysik sei „tuned to handle and feel like their real-world counterparts“.
- **Epic Content License** (gilt für Legacy-Megascans und bestimmte Epic-Inhalte): Objektcode-Verbreitung „only as an inseparable part of a Project“. Es gibt eine Kategorie „UE-Only Content“. Volltext auf [unrealengine.com/eula/content](https://www.unrealengine.com/en-US/eula/content). Der Abruf wurde blockiert, die Zitate stammen aus Suchmaschinen-Auszügen der Seite **[teilweise UNVERIFIZIERT]**.

### 1.3 Sketchfab

**Lizenztypen**, abgerufen direkt aus der öffentlichen [Sketchfab-API `/v3/licenses`](https://api.sketchfab.com/v3/licenses):

| Slug | Label | Sketchfab-Beschreibung | Für uns |
|---|---|---|---|
| `by` | CC Attribution (4.0) | „Author must be credited. Commercial use is allowed.“ | ✅ |
| `cc0` | CC0 Public Domain | „Credit is not mandatory. Commercial use is allowed.“ | ✅ |
| `by-sa` | CC Attribution-ShareAlike | „Modified versions must have the same license.“ | ❌ (Fab-Konflikt, s. §1.1) |
| `by-nd` | CC Attribution-NoDerivs | „Modified versions can not be distributed.“ | ❌ |
| `by-nc`, `by-nc-sa`, `by-nc-nd` | NonCommercial-Varianten | „No commercial use.“ | ❌ |
| `free-st` / `st` | (Free) Standard | „use worldwide, on all types of media, commercially or not, and in all types of derivative works“ | ⚠️ Sketchfabs eigene Standard-Lizenz ist **nicht** die Fab Standard License. Nach DESIGN.md nicht freigegeben, Volltext [sketchfab.com/licenses](https://sketchfab.com/licenses) |
| `ed` | Editorial | „Use only in connection with events that are newsworthy or of public interest“ | ❌ |

- **Sketchfab Standard/Editorial** ([sketchfab.com/licenses](https://sketchfab.com/licenses)): Editorial-Assets „cannot be used for any commercial or promotional use“. Bei Standard gilt: „you may not use the 3D asset in a way that allows others to use or access the 3D asset as a stand-alone file.“
- **Der Sketchfab Store ist geschlossen**, Käufe laufen über Fab. Freie Modelle bleiben vorerst auf Sketchfab, mit dem Versprechen „plenty of notice before free downloadable content is no longer supported“ ([Sketchfab-Blog, 22.10.2024](https://sketchfab.com/blogs/community/sketchfab-update-what-you-need-to-know-now-that-fabs-live/); [Help Center](https://help.sketchfab.com/en/articles/16152225-store-license-usage-faq)). ⇒ **Gewählte Modelle früh sichern** und Lizenz plus Seite als Screenshot/PDF archivieren.
- **NoAI-Tag:** Seit 21.11.2023 nur noch bei Standard-lizenzierten Modellen, nicht bei CC ([Sketchfab-Blog](https://sketchfab.com/blogs/community/restricting-generative-ai-use-of-free-models)).
- **Namensnennung laut Sketchfab:** z. B. „This work is based on *Model* by *Author* licensed under CC BY 4.0“ ([Help Center](https://help.sketchfab.com/en/articles/16152215-crediting-users-for-3d-model-downloads)).

### 1.4 CC BY 4.0 in einem kompilierten Spiel

Primärtext: [CC BY 4.0 Legal Code](https://creativecommons.org/licenses/by/4.0/legalcode.en) und [CC FAQ](https://creativecommons.org/faq/).

- **Ja, erlaubt.** §2(a)(4): Ausübung „in all media and formats whether now known or hereafter created“, inklusive technischer Änderungen (Konvertierung in `.uasset`, Kochen, Packen in `.pak`). Kommerzielle Nutzung ist nicht eingeschränkt.
- **Pflichten (§3(a)):** Urheber nennen, Copyright-Hinweis, Hinweis auf die Lizenz, Link/URI zum Material „to the extent reasonably practicable“ und Änderungen kenntlich machen („indicate if You modified the Licensed Material“). Form: „in any reasonable manner based on the medium, means, and context“. ⇒ Credits-Bildschirm im Spiel **plus** `CREDITS.md`/`CREDITS.txt` im Spielordner.
- **DRM-Klausel (§2(a)(5)(C))**: „You may not … apply any Effective Technological Measures to, the Licensed Material if doing so restricts exercise of the Licensed Rights by any recipient“. Laut CC FAQ ist bloßes Konvertieren in ein schwer zugängliches Format kein Verstoß. Für eine **kostenlose Verbreitung ohne DRM** ist das unproblematisch. Bei einem späteren Verkauf mit DRM (z. B. Steam-DRM, verschlüsselte Paks) ist die Bewertung für CC-BY-Teile **[UNVERIFIZIERT / rechtlich prüfen]**. CC0-Assets haben dieses Problem nicht.
- **Marken sind ausgenommen:** „Patent and trademark rights are not licensed under this Public License“ (§2(b)(2)). Ein CC-BY-Automodell mit Ferrari-Logo erlaubt also *nicht* die Nutzung des Ferrari-Logos.

### 1.5 Freesound

[Freesound FAQ](https://freesound.org/help/faq/): Lizenz **pro Sound**. Mögliche Lizenzen: CC0, CC BY, CC BY-NC und das alte „Sampling+“ (ähnlich NC, keine Werbung). ⇒ **Nur CC0 und CC BY nehmen.** Ältere Sounds stehen teils unter **CC BY 3.0** (z. B. [hantorio](https://freesound.org/people/hantorio/sounds/274907/)). Die Pflichten sind dort praktisch dieselben. Die FAQ empfiehlt für viele Sounds eine verlinkte Credits-Seite. Gen-AI-Präferenzen der Uploader sind ein separates Thema (gleiche FAQ).

### 1.6 Poly Haven und ambientCG

- **Poly Haven** ([polyhaven.com/license](https://polyhaven.com/license)): „Our assets are all licensed as CC0“, nutzbar „for any purpose“ inklusive kommerziell, „You do not need to give credit.“ Ausgenommen sind Logos und Website-Inhalte der Plattform selbst.
- **ambientCG** ([docs.ambientcg.com/license](https://docs.ambientcg.com/license/)): „All ambientCG assets are provided under the Creative Commons CC0 1.0 Universal License“. Ausdrücklich: „You can include the raw files in your project, for example a video game.“ Vorgeschlagener (freiwilliger) Credit: „Created using <asset name> from ambientCG.com, licensed under the Creative Commons CC0 1.0 Universal License.“

### 1.7 Weitere geprüfte Quellen

- **Kenney Car Kit** ([kenney.nl/assets/car-kit](https://kenney.nl/assets/car-kit)): CC0, Low-Poly-Stil. Nur als Prototyp-Platzhalter, nicht fotorealistisch.
- **Khronos glTF-Sample-Assets**: „CarConcept“ steht unter **CC BY 4.0** (Artist Eric Chadwick, Owner Darmstadt Graphics Group), **enthält aber Khronos-Logos**, die als Marke separat lizenziert sind ([metadata.json](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/CarConcept/metadata.json)). „ToyCar“ ist CC0 ([metadata.json](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/ToyCar/metadata.json)).
- **Sonniss GDC Game Audio Bundle** ([sonniss.com/gameaudiogdc](https://sonniss.com/gameaudiogdc)): „royalty free and commercially usable“, „No attribution is required“, nur für Medienproduktion, keine Weitergabe als Einzeldateien, „Use for AI/ML training is strictly prohibited“. Das ist eine **eigene Lizenz**, also nach DESIGN.md nicht freigegeben. Qualitativ ist es eine der besten Gratis-Quellen für Fahrzeugaufnahmen. **Entscheidung nötig**, ob die Lizenzliste erweitert wird.
- **engine-sim** von Ange Yaghi ([GitHub, MIT-Lizenz laut GitHub-API](https://github.com/ange-yaghi/engine-sim)): Simulator, der Motorsound aus Motorgeometrie erzeugt. Die MIT-Lizenz gilt für den Code. Dass die **erzeugten Audiodateien** frei nutzbar sind (Rechte beim Anwender), ist plausibel, aber **[UNVERIFIZIERT]**: In der Lizenz steht dazu nichts Ausdrückliches.

---

## 2. Konkrete Kandidaten

**Legende:** ✅ = Lizenz passt, Design fiktiv. ⚠️ = Lizenz passt, aber Risiko (Designvorlage, Ähnlichkeit, Herkunft). ❌ = nicht verwenden.
Alle Sketchfab-Angaben stammen aus der [Sketchfab-API](https://api.sketchfab.com/v3/models/) (Abruf 2026-10-11). „Faces“ ist die Dreieckszahl laut API. **Vor dem Download Lizenz auf der Seite erneut prüfen und Screenshot archivieren.**

### 2.1 Autos

Zur Einordnung: Eine Breitensuche (26 Suchbegriffe, CC0 und CC BY, Kategorie Cars & Vehicles) ergibt fast nur echte Marken (Porsche, BMW, Lamborghini …) oder Fan-Nachbauten aus anderen Spielen. Fiktive, hochwertige Modelle **mit Interieur** sind die Ausnahme.

| # | Name | Link | Autor | Lizenz (Seite) | Qualität / Hinweise | Bewertung |
|---|---|---|---|---|---|---|
| 1 | **Gourami – Luxury Saloon** | [sketchfab.com/3d-models/bf29aca039c447ab913608e3f6818875](https://sketchfab.com/3d-models/bf29aca039c447ab913608e3f6818875) | Fishboe (ministephen) | CC Attribution | 107k Faces, eigener Entwurf („I designed my own luxury car“), V12-Limousine. Interieur nicht angegeben **[UNVERIFIZIERT]** | ✅ |
| 2 | **Fictional supercar – V12 Goblin** | [sketchfab.com/3d-models/0a20e49ad5774d778567cb5c3f345786](https://sketchfab.com/3d-models/0a20e49ad5774d778567cb5c3f345786) | Olli Teittinen (ollitei) | CC Attribution | 87k Faces, eigener Entwurf von 2012, „Unique exterior. **No interior.**“ Hochwertiges Design. Achtung, es gibt Re-Uploads durch Dritte (s. §3.1), nur das Original nehmen | ✅ (ohne Cockpit) |
| 3 | **Vintage Racing Car** | [sketchfab.com/3d-models/0b4cd62e8620486f9b62c32d87d3ed0e](https://sketchfab.com/3d-models/0b4cd62e8620486f9b62c32d87d3ed0e) | SpatialNeglect (jeandiz) | CC Attribution | 86k Faces, PBR 4K, Tags „interior“, „concept“, für einen Wettbewerb gebaut. Auch auf Fab gelistet ([Fab](https://www.fab.com/listings/e05614bb-f53f-4837-ae23-fdb10bb15be2), Lizenz dort **[UNVERIFIZIERT]**) | ✅ |
| 4 | **Orion Skylark GT** | [sketchfab.com/3d-models/b1cc3110ed6e41248b0ccadb28e41e54](https://sketchfab.com/3d-models/b1cc3110ed6e41248b0ccadb28e41e54) | dark_igorek | CC Attribution | 493k Faces (High-Poly, braucht LODs/Retopo), Retro-GT, Tag „originaldesign“, 2026. „inspired by a sketch by Fernando Correa“. Ob der Konzeptkünstler zugestimmt hat: **[UNVERIFIZIERT]** | ⚠️ |
| 5 | **Retro coupe** | [sketchfab.com/3d-models/b356898aa8e246fd9c59f6b8abd36791](https://sketchfab.com/3d-models/b356898aa8e246fd9c59f6b8abd36791) | zhe_kan | CC Attribution | 157k Faces, „Fictional design, inspired by MG, Shelby, Ferrari cars of the 50s and 60s“. Mischung statt Kopie, aber vor Nutzung auf zu große Nähe zu einem Einzelmodell prüfen | ⚠️ (vermutlich ok) |
| 6 | **Muscle Car Concept** | [sketchfab.com/3d-models/aa5dbdc7d492478ababfd4d385b8339f](https://sketchfab.com/3d-models/aa5dbdc7d492478ababfd4d385b8339f) | ViktorKom | CC Attribution | 137k Faces, 2× 4K-Texturen, „Game-ready“, laut Autor „ready for Unity, UE“. Inspiriert von Camaro 69, Challenger, Mustang GT und anderen (Mischung) | ⚠️ (vermutlich ok) |
| 7 | **VAW Kaveria Gran Coupe M-2K** | [sketchfab.com/3d-models/73b4c8631b184185b16508e687620533](https://sketchfab.com/3d-models/73b4c8631b184185b16508e687620533) | Glas'kavin (fiendishfarlander76) | CC Attribution | 314k Faces, fiktive Marke „Vauxhell Automotive Works“. **Der Name spielt auf „Vauxhall“ an und muss umbenannt werden.** Form gegen echte Vorbilder prüfen | ⚠️ |
| 8 | **Furai Eclipse GT-W16** | [sketchfab.com/3d-models/c4c7c8381b6244af8ba221ec843c1758](https://sketchfab.com/3d-models/c4c7c8381b6244af8ba221ec843c1758) | Glas'kavin | CC Attribution | 195k Faces, „Fully rigged … ready for UE5“, Track-Car. Laut Autor „Inspired (very) by the Lamborghini Essenza SCV12“, also **hohes Formrisiko** | ⚠️ / eher ❌ |
| 9 | **Tokii Abyss GT.7 V12** | [sketchfab.com/3d-models/c399568541104e0e903cf138b1e24908](https://sketchfab.com/3d-models/c399568541104e0e903cf138b1e24908) | Glas'kavin | CC Attribution | 158k Faces, rigged für UE5, „inspired by the McLaren Sabre“, also **Formrisiko** | ⚠️ |
| 10 | **CarConcept** (Khronos) | [GitHub](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/CarConcept) | Eric Chadwick / Darmstadt Graphics Group | CC BY 4.0 | Fiktives Konzeptauto mit Material-Varianten (Showcase-Asset). **Khronos-Logos entfernen** (Markenrecht). Interieur **[UNVERIFIZIERT]** | ✅ (nach Logo-Entfernung) |
| 11 | **City Sample Vehicles** | [Fab](https://www.fab.com/listings/2909157b-ddfa-4cef-a925-69dc2467021f) | Epic Games | Fab, UE-Only, kostenlos | 13 fahrbare, fiktive Alltagsfahrzeuge, UE-fertig. Gut für KI-Feld/Tests, nicht für den GT-Sportwagen-Look | ✅ |
| 12 | **UE Vehicle Template: SportsCar, OffroadCar** | [Epic-Doku](https://dev.epicgames.com/documentation/unreal-engine/vehicle-template-in-unreal-engine) | Epic Games | UE EULA „Examples“ | Sofort fahrbar, ideal für **M1**. Optisch nicht auf Endniveau | ✅ (Platzhalter) |
| 13 | **Generic passenger car pack** | [sketchfab.com/3d-models/20f9af9b8a404d5cb022ac6fe87f21f5](https://sketchfab.com/3d-models/20f9af9b8a404d5cb022ac6fe87f21f5) | Comrade1280 | CC Attribution | Low-Poly (je ca. 7k Tris), 10 Typen, „completly trademark clame proof“ (Autorenaussage). Nur Hintergrund/Parkplatz | ✅ (Deko) |
| 14 | **Daniel-Zhabotinsky-Serie** (z. B. [Fairheaven LT '80](https://sketchfab.com/3d-models/e2678da920cc4be68dbc193727919ffb), [Kiri '86](https://sketchfab.com/3d-models/5c17c582ef9549798f694b660feea42a)) | s. Links | Daniel Zhabotinsky | CC Attribution | Low-Poly (ca. 20k), fiktive Namen. Zusätzlich: „I unconditionally give you permission to use this model … implement it into a digital product.“ „Tiara GT '83“ ist erkennbar AE86/Initial-D-inspiriert ⚠️ | ✅ (Deko/Fernbereich) |
| 15 | **Kenney Car Kit** | [kenney.nl/assets/car-kit](https://kenney.nl/assets/car-kit) | Kenney | CC0 | Stilisiert, nur Prototyp | ✅ (Platzhalter) |

**Nicht verwenden (Beispiele aus der Suche):**
- **Borchia V8 VC** (ollitei): heute „**Editorial**“ und nicht mehr downloadbar ([Seite](https://sketchfab.com/3d-models/cd4ffabca37840cfa49f3d8f6742e448)). ❌
- **Italia Velocita, Patriot P10-XR, Ozutsu Ronin, Jester Butane, Rayback Mohawk** (Swiss_Fox): CC Attribution, aber **Nachbauten von Fahrzeugen aus *MotorStorm* bzw. *Split/Second*** („features every exterior detail very close to the original (Including the fictional brand stickers)“, [Beispiel](https://sketchfab.com/3d-models/1696b756b09644bf924d31559287b70f)). Die Designs gehören den Spielestudios, die CC-Lizenz des Uploaders deckt sie nicht. ❌
- **Kralzfiller-Modelle** (Tyler_Dave, z. B. [Bicktacollecto](https://sketchfab.com/3d-models/a02acd75e3cb4813a0f5ea79c6fec9a7)): als fiktiv beschrieben, aber derselbe Account lädt mit „Ripped“/„asseto-corsa“ getaggte Strecken unter CC-BY hoch (s. §3.1). Herkunft nicht vertrauenswürdig. ❌
- **Future Car** (3DHaupt, [Seite](https://sketchfab.com/3d-models/hliAt7xc6YU2XjXh5G4lzmQwFmR)): „The concept was created by Piotr Kupsc“. Rechte am Konzept **[UNVERIFIZIERT]**. Außerdem 2013, eher Sci-Fi. ⚠️
- Alles mit echtem Markennamen (Porsche, BMW, Lamborghini, Corvette …), auch wenn die Lizenz „CC Attribution“ lautet (s. §3).

**Fazit Autos:** Für die Meilenstein-Version mit 3 Autos realistisch: **Vintage Racing Car**, **Gourami** und **V12 Goblin** (dann ohne Cockpitkamera), plus Vehicle Template / City Sample für Tests und KI. Für v1.0 mit 10–12 Autos braucht es entweder eigene Modelle oder **bezahlte Fab-Modelle mit Standard License**. Letzteres verstößt gegen „nur kostenlos“, das müsste bewusst entschieden werden.

### 2.2 Strecke und Umgebung

#### Asphalt, Straße, Randbereiche: Texturen/Materialien (alle CC0)

| Asset | Link | Autor | Notiz |
|---|---|---|---|
| **Asphalt Track** | [polyhaven.com/a/asphalt_track](https://polyhaven.com/a/asphalt_track) | Dimitrios Savva | bis 8K, explizit Rennstrecken-Asphalt |
| **Asphalt Pit Lane** | [polyhaven.com/a/asphalt_pit_lane](https://polyhaven.com/a/asphalt_pit_lane) | Dimitrios Savva | Boxengasse |
| Clean Asphalt, Asphalt 01–07, Road Damaged | [polyhaven.com/textures](https://polyhaven.com/textures) (IDs `clean_asphalt`, `asphalt_04`, `asphalt_07` (16K), `road_damaged`) | div. | Variation, Patches, Abnutzung |
| Gravel Road, Rocky Gravel, Sparse Grass, Grass Path 2/3, Forest Ground 04–06, Rocky Terrain 01–03 | polyhaven.com/a/`<id>` | div. | Kiesbett, Auslaufzonen, Böschungen |
| **ambientCG Road001–Road015, Asphalt009–033** | z. B. [ambientcg.com/view?id=Road007](https://ambientcg.com/view?id=Road007), [Asphalt033](https://ambientcg.com/view?id=Asphalt033) | ambientCG (Lennart Demes) | Straßen teils mit Markierungen |
| ambientCG Grass001–008, Ground0xx, Gravel0xx, Rock0xx | z. B. [Grass004](https://ambientcg.com/view?id=Grass004), [Gravel023](https://ambientcg.com/view?id=Gravel023) | ambientCG | Terrain-Layer |

(Asset-IDs aus der [Poly Haven API](https://api.polyhaven.com/assets?t=textures) und der [ambientCG API](https://ambientcg.com/api/v2/full_json?q=asphalt&type=Material), Abruf 2026-10-11.)

#### Barrieren, Curbs, Streckenmobiliar

| Asset | Link | Autor | Lizenz | Notiz |
|---|---|---|---|---|
| Concrete Road Barrier 01/02 | [polyhaven.com/a/concrete_road_barrier](https://polyhaven.com/a/concrete_road_barrier) | Amal Kumar | CC0 | Betonelemente (Jersey-Typ) |
| Old Tyre, Street Lamp 01/02, Utility Box | [polyhaven.com/a/old_tyre](https://polyhaven.com/a/old_tyre) usw. | MP, Josh Dean u. a. | CC0 | Deko |
| **Race Track Assets Pack** | [sketchfab.com/3d-models/4a863e7005d647adb2f72775cf41e808](https://sketchfab.com/3d-models/4a863e7005d647adb2f72775cf41e808) | Cherk (alex20010804) | CC Attribution | Betonbarriere, Reifenstapel, Zaun, Leitplanke, Bremsmarker 50–200 m, Lampen, Kegel; 2K PBR. Tags enthalten „gran turismo“, also vorher auf Rip prüfen ⚠️ |
| Metal And Concrete Guardrail | [sketchfab.com/3d-models/a50e5b5cddc74d618b29391168eaab8f](https://sketchfab.com/3d-models/a50e5b5cddc74d618b29391168eaab8f) | Mehdi Shahsavan | CC Attribution | Leitplanke |
| Barrier & Traffic Cone Pack 1+2 | [Teil 1](https://sketchfab.com/3d-models/23c4dfca76a24bf0b21894847867af2a), [Teil 2](https://sketchfab.com/3d-models/426e2d6eb39b4947856332aec6be4fb4) | Sabri Ayeş | CC Attribution | Low-Poly-Deko |
| **Curbs (rot-weiß)** | – | – | – | Kein überzeugendes freies Asset gefunden. **Empfehlung:** als Spline-Mesh selbst bauen (Geometrie trivial), Material aus Poly Haven `painted_concrete` plus eigener Maske |

#### Vegetation, Terrain, Felsen

| Asset | Link | Lizenz | Notiz |
|---|---|---|---|
| Poly Haven Bäume: `pine_tree_01`, `fir_tree_01`, `jacaranda_tree`, `island_tree_01–03`, Sträucher `shrub_01–04`, Gras `grass_medium_01/02`, `grass_bermuda_01`, Felsen `rock_face_01/02`, `boulder_01`, `mountainside`, `coastal_cliff_*` | [polyhaven.com/models/nature](https://polyhaven.com/models/nature) | CC0 | scan-basiert, sehr hohe Qualität. Für Nanite/LOD vorbereiten |
| Megascans (Fab, kostenlose Listings) | [fab.com, Filter Free + Quixel](https://www.fab.com/sellers/Quixel) (URL **[UNVERIFIZIERT]**) | Fab Standard | **Pro Listing prüfen**, was aktuell gratis ist **[UNVERIFIZIERT]** |
| Epic „Electric Dreams“-Sample (UE5, Megascans-Vegetation, PCG) | Fab | vermutlich Fab Standard/UE-Only **[UNVERIFIZIERT]** | Als Referenz für fotorealistische Landschaft in UE5 sehr wertvoll |

#### HDRIs (alle CC0, Poly Haven)

| HDRI | Link | Stimmung |
|---|---|---|
| Kloofendal 48d Partly Cloudy (Pure Sky) | [polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky](https://polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky) | Mittag, Wolken, 16K |
| Kloofendal 43d Clear (Pure Sky) | [polyhaven.com/a/kloofendal_43d_clear_puresky](https://polyhaven.com/a/kloofendal_43d_clear_puresky) | Mittag, klar, 24K |
| Kloppenheim 06 (Pure Sky) | [polyhaven.com/a/kloppenheim_06_puresky](https://polyhaven.com/a/kloppenheim_06_puresky) | Sonnenuntergang |
| Autumn Field (Pure Sky) | [polyhaven.com/a/autumn_field_puresky](https://polyhaven.com/a/autumn_field_puresky) | Vormittag, klar |
| Overcast Soil (Pure Sky) | [polyhaven.com/a/overcast_soil_puresky](https://polyhaven.com/a/overcast_soil_puresky) | bewölkt |
| Studio: `studio_small_09`, `photo_studio_01` | [polyhaven.com/a/studio_small_09](https://polyhaven.com/a/studio_small_09) | Garage/Showroom/Fotomodus |

„Pure Sky“-Varianten haben keinen Bodenanteil und passen gut als Himmel zu einer festen Tageszeit pro Strecke.

### 2.3 Sound

Lizenz pro Sound direkt auf der Seite geprüft (CC-Link im HTML, Abruf 2026-10-11). **Nicht angehört.** Echte Markennamen in den Dateititeln sind unkritisch, solange sie im Spiel nicht auftauchen.

#### Motor-Loops / Drehzahl-Material

| Sound | Link | Autor | Lizenz | Notiz |
|---|---|---|---|---|
| **Car_Sound_BMW_120d (Pack)**: Idle/RPMLow/RPMMed/RPMHigh, innen + außen, Start/Stop | [Pack 22622](https://freesound.org/people/GiocoSound/packs/22622/) (z. B. [RPMHigh außen](https://freesound.org/people/GiocoSound/sounds/401551/)) | GiocoSound | **CC0** | Direkt 4 Drehzahlstufen × 2 Perspektiven, ideal für einen ersten MetaSound-Crossfade. Diesel-R4 |
| car_idle_ext_loop + Pack „Car sounds“ | [369054](https://freesound.org/people/AndrewAlexander/sounds/369054/) | AndrewAlexander | CC0 | Leerlauf-Loop |
| Engine Contact Recording – Mini Cooper S 2019 (3 Sounds: Basis, Gunning it, Gear Shift) | [Pack 37350](https://freesound.org/people/TheLittleCrow/packs/37350/) | TheLittleCrow | CC0 | Kontaktmikrofon, R4 Turbo. Gangwechsel-Material |
| HYUNDAI KONA Engine Static 3000Rpm Interior (+ Pack) | [845610](https://freesound.org/people/KVV_Audio/sounds/845610/), [Pack 45205](https://freesound.org/people/KVV_Audio/packs/45205/) | KVV_Audio | CC BY 4.0 | Statische Drehzahl, innen. Weitere RPM-Stufen im Pack **[UNVERIFIZIERT]** |
| FR-S BRZ GT-86 Vortech Supercharged Dyno Run | [191194](https://freesound.org/people/dezoris/sounds/191194/) | dezoris | CC BY 4.0 | **Boxer**-Volllast-Sweep, in Drehzahl-Scheiben schneidbar |
| 1953 Studebaker Idle up to 6000rpm | [238847](https://freesound.org/people/lonemonk/sounds/238847/) | lonemonk | CC BY 4.0 | Sweep, Oldtimer-R6/V8 **[Motortyp UNVERIFIZIERT]** |
| Porsche 911 revving (boxer) | [772802](https://freesound.org/people/jerry.berumen/sounds/772802/) | jerry.berumen | CC BY 4.0 | Boxer-Revs (Standgas) |
| v8 engine rev | [651534](https://freesound.org/people/overmedium/sounds/651534/) | overmedium | CC0 | V8-Revs |
| Big Block V8 Rev 2 | [127981](https://freesound.org/people/Henaway/sounds/127981/) | Henaway | CC0 | V8, MP3 (Qualität prüfen) |
| V12 Engine – Short Rev | [484191](https://freesound.org/people/Janosch-JR/sounds/484191/) | Janosch-JR | CC0 | V12 |
| WRX – Exhaust sounds | [273334](https://freesound.org/people/ulose2piranha/sounds/273334/) | ulose2piranha | CC0 | Boxer-Turbo |
| Eight-cylinder engine idling | [636066](https://freesound.org/people/Lumamorph/sounds/636066/) | Lumamorph | CC BY 4.0 | V8-Leerlauf |
| EngineWarmup_1-Loop (+ Pack „Cars and Engines“) | [557214](https://freesound.org/people/LHermanns/sounds/557214/) | LHermanns | CC BY 4.0 | Loop |
| Mercedes 180D low/medium rpm loop | [274907](https://freesound.org/people/hantorio/sounds/274907/) | hantorio | CC BY 3.0 | Diesel-Loop |
| **engine-sim** (Generator) | [GitHub](https://github.com/ange-yaghi/engine-sim) | Ange Yaghi | MIT (Code) | Konstante Drehzahl-Loops für R4/V6/V8/Boxer selbst rendern. Rechte am Output **[UNVERIFIZIERT]** |

**Realistische Einschätzung:** Ein GT-taugliches Motor-Set (Volllast und Schiebebetrieb, je 6–10 Drehzahlstufen, innen und außen) gibt es frei kaum. Wege dahin:
(a) Sweeps (Dyno-Run) in kurze Drehzahl-Scheiben schneiden und granular abspielen,
(b) engine-sim-Renderings,
(c) eigene Aufnahmen mit Kontaktmikro an einem Bekanntenauto,
(d) Lizenzerweiterung (Sonniss) entscheiden.

#### Reifen, Wind, Rollen, Effekte

| Sound | Link | Autor | Lizenz |
|---|---|---|---|
| tire-squeal | [850041](https://freesound.org/people/JeffPearson/sounds/850041/) | JeffPearson | CC0 |
| Chrysler LHS tire squeal 01–04 | [71736](https://freesound.org/people/audible-edge/sounds/71736/), [71737](https://freesound.org/people/audible-edge/sounds/71737/) … | audible-edge | CC0 (geprüft: 71737; die anderen **[UNVERIFIZIERT]**) |
| screeching tyres | [614627](https://freesound.org/people/johnnydekk/sounds/614627/) | johnnydekk | CC0 |
| Nordschleife VLN race (7 Sounds: Pass-bys, Reifenquietschen, Fehlzündungen) | [Pack 19938](https://freesound.org/people/Dominik_W/packs/19938/) | Dominik_W | CC0 (geprüft: 350677) |
| Car rolling on asphalt | [591079](https://freesound.org/people/orlandorizo/sounds/591079/) | orlandorizo | CC0 |
| Wind inside car | [82267](https://freesound.org/people/ra_gun/sounds/82267/) | ra_gun | CC0 |
| Turbo Spooling and Blow Off | [659544](https://freesound.org/people/EwanPenman11/sounds/659544/) | EwanPenman11 | CC0 |
| Backfire | [446101](https://freesound.org/people/JustInvoke/sounds/446101/) | JustInvoke | CC BY 4.0 |
| Brake skid gravel | [637161](https://freesound.org/people/kyles/sounds/637161/) | kyles | CC0 |
| Passing Car (Wet road) | [462862](https://freesound.org/people/Breviceps/sounds/462862/) | Breviceps | CC0 |

---

## 3. Fallstricke

### 3.1 Gerippte Spielmodelle mit falscher CC-Lizenz

- **Belegtes Beispiel:** Sketchfab-Account *Tyler_Dave* („Dave Love SketchFab“) lädt „Marina Bay Street Circuit“ unter **CC Attribution** hoch, mit den Tags **„Ripped“** und **„asseto-corsa“**, und verlinkt den Download auf mega.nz ([Seite](https://sketchfab.com/3d-models/e665d831c57546609d4c15287a6107b7); Tags über die [API](https://api.sketchfab.com/v3/models/e665d831c57546609d4c15287a6107b7)). Derselbe Account hat weitere echte Strecken (Red Bull Ring, Daytona) und echte Rennwagen (Alpine A424, „DTM Pack“, Haas VF-26) unter CC-BY. ⇒ **Die CC-Lizenz einer Sketchfab-Seite ist nur eine Behauptung des Uploaders.** Wer nicht Rechteinhaber ist, kann keine Lizenz erteilen.
- **Re-Uploads:** „V12 Goblin“ von ollitei taucht mit identischer Polygonzahl (86.623) bei mindestens zwei fremden Accounts auf ([Beispiel 1](https://sketchfab.com/3d-models/0a235e554ea94455bafd676c66749882), [Beispiel 2](https://sketchfab.com/3d-models/7eb0880e094945e386159e7590c4c451)). Immer das **Original** verwenden und den **Originalautor** nennen.
- **Fan-Nachbauten** fiktiver Autos aus anderen Spielen (MotorStorm, Split/Second, GTA, Rocket League, Speed Racer) sind nicht „fiktiv und frei“: Das Design gehört dem jeweiligen Studio (s. §2.1).

**Warnsignale:** Name eines echten Fahrzeugs oder einer echten Strecke. Tags wie „ripped“, „forza“, „gt7“, „assetto“, „nfs“. Extrem viele Uploads in kurzer Zeit. Download-Link außerhalb von Sketchfab. Auffällige Textur-Atlanten oder LOD-Ketten im Stil eines Spiels. Kein Wireframe/WIP. Beschreibung kopiert aus Wikipedia.
**Gegenprobe:** Profil des Autors prüfen (Portfolio, ArtStation, WIP-Bilder), Bild-Rückwärtssuche, Datum des Erst-Uploads.

### 3.2 Marken, Logos, Karosserieformen: Reicht Entlogoisieren?

**Kurz: Logos und Namen zu entfernen ist notwendig, aber nicht hinreichend, wenn die Form ein echtes Modell erkennbar kopiert.**

- **CC-Lizenzen umfassen keine Marken** (CC BY 4.0 §2(b)(2), [Legal Code](https://creativecommons.org/licenses/by/4.0/legalcode.en); [CC FAQ](https://creativecommons.org/faq/): „trademark rights are not licensed by the CC licenses“).
- **EU, Logos auf originalgetreuen Nachbildungen:** Der EuGH hat in *Adam Opel AG v. Autec AG* (C-48/05, 2007) entschieden, dass das Opel-Logo auf einem originalgetreuen Spielzeugmodell eine Markenverletzung sein **kann**: wenn es die Funktion der Marke für Spielzeug beeinträchtigt, oder bei bekannten Marken, wenn es ohne rechtfertigenden Grund deren Ruf ausnutzt. Die Schutzschranke „Angabe über Merkmale“ greift ausdrücklich **nicht** ([EUR-Lex, CELEX 62005CJ0048](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:62005CJ0048)). ⇒ **Logos, Schriftzüge und Modellnamen konsequent entfernen.** Auch Emblem-Formen auf Felgen, Lenkrad und Reifenflanken (Reifenmarken!).
- **USA, Karosserieform als Trade Dress:** In *Ferrari S.p.A. v. Roberts*, 944 F.2d 1235 (6th Cir. 1991), wurde die Außenform von Daytona Spyder und Testarossa als **ungeschützt-eingetragene Trade Dress** anerkannt. Glasfaser-Nachbauten auf fremdem Chassis wurden untersagt ([Volltext](https://law.resource.org/pub/us/case/reporter/F2/944/944.F2d.1235.90-5734.html)). ⇒ Die *Form* allein kann markenrechtlich geschützt sein. Ob das für digitale Darstellungen in Spielen genauso gilt, ist nicht entschieden **[UNVERIFIZIERT]**.
- **USA, Spiele und echte Fahrzeuge:** *AM General v. Activision* (S.D.N.Y. 2020): Humvees in *Call of Duty* waren über den Rogers-Test (künstlerische Relevanz, nicht ausdrücklich irreführend) geschützt ([Finnegan-Zusammenfassung](https://www.finnegan.com/en/insights/blogs/incontestable/in-legal-warfare-over-humvee-trademarks-the-first-amendment-goes-beyond-the-call-of-duty-in-dismissing-am-generals-claims.html); Primärurteil nicht abgerufen **[UNVERIFIZIERT am Primärtext]**). Der Supreme Court hat den Rogers-Test 2023 in *Jack Daniel's v. VIP Products* eingeschränkt ([Urteil 22-148](https://www.supremecourt.gov/opinions/22pdf/22-148_3e04.pdf), nicht im Detail ausgewertet **[UNVERIFIZIERT]**). Für ein Hobbyprojekt ist „wir würden vor Gericht wohl gewinnen“ ohnehin keine Strategie.
- **EU, Designschutz gilt jetzt auch digital:** Die Gemeinschaftsgeschmacksmuster-VO (EU) 6/2002 in der Fassung von VO (EU) 2024/2822 definiert „Erzeugnis“ als Gegenstand „regardless of whether it is embodied in a physical object or materialises in a non-physical form“ (Art. 3). Zu den verbotenen Handlungen zählt das Erstellen und Verbreiten von Software, die das Design aufzeichnet, um ein verletzendes Erzeugnis herzustellen (Art. 19(2)(d)) ([EUR-Lex 32024R2822](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32024R2822)). EUR-Lex vermerkt, dass der Rechtsakt durch VO (EU) 2026/715 abgelöst bzw. kodifiziert wurde (Stand und Inhalt **[UNVERIFIZIERT]**). Eingetragene Designs von Autos sind üblich. Ein naher digitaler Nachbau eines aktuellen Modells ist daher in der EU ein **reales Risiko**. Wie weit das für Spielemodelle reicht, ist gerichtlich ungeklärt **[UNVERIFIZIERT]**.
- **Für APEXLINE heißt das** (abgeleitet, keine Rechtsberatung):
  1. Keine Modelle verwenden, die als ein bestimmtes echtes Auto *gemeint* sind, auch nicht entlogoisiert („Porsche 911 GT3“ ohne Wappen bleibt ein 911).
  2. „Erkennbar inspiriert“ (DESIGN.md) über *Gattung* lösen (Mittelmotor-Keil, 70er-Coupé, Le-Mans-Prototyp), **nicht über Signatur-Elemente**: markante Grills (z. B. Niere, Schild), typische Leuchtengrafiken, charakteristische Seitenlinien.
  3. Fiktive Namen per Markensuche prüfen ([EUIPO eSearch](https://euipo.europa.eu/eSearch/), [DPMAregister](https://register.dpma.de/)). Wortspiele wie „Vauxhell“ oder „Lamborgotti“ vermeiden.
  4. Reifenflanken, Bremssättel, Sponsoraufkleber, Kennzeichen: alles fiktiv.
  5. Mischformen aus mehreren Vorbildern (wie #5 und #6 oben) sind deutlich risikoärmer als Einzelkopien.

### 3.3 KI-generierte Assets

- **Urheberrecht:** Laut US Copyright Office, Report „Copyright and Artificial Intelligence, Part 2: Copyrightability“ (29.01.2025), sind rein KI-generierte Inhalte ohne ausreichenden menschlichen Beitrag nicht schutzfähig. Prompts allein reichen nicht ([copyright.gov NewsNet 1060](https://copyright.gov/newsnet/2025/1060.html)). Folgen: (a) Eine CC-BY-„Lizenz“ auf einem rein KI-generierten Modell ist möglicherweise gegenstandslos. Das ist für uns harmlos, aber (b) das Modell kann **Fremdmaterial reproduzieren** (Trainingsdaten, z. B. echte Autoformen oder Logos). Dieses Risiko trägt der Nutzer. Für die EU/DE gibt es keine geprüfte Primärquelle **[UNVERIFIZIERT]**.
- **Fab:** Anbieter müssen KI-generierten Content mit „CreatedWithAI“ taggen ([Fab EULA PDF](https://cdn1.epicgames.com/eulatracking-download/emporium/en/v4/r1/2b42ccdae5f8a8b42f1d7e7f869e4c9c.pdf)). Das Feld „Generated with AI“ steht auf jedem Listing ([Beispiel City Sample Vehicles](http://web.archive.org/web/20251113193215/https://www.fab.com/listings/2909157b-ddfa-4cef-a925-69dc2467021f)).
- **Sketchfab** hat keine vergleichbare Pflichtkennzeichnung **[UNVERIFIZIERT]**. Verdächtig sind sehr generische, „marketingartige“ Beschreibungen und Massen-Uploads (vgl. die Kralzfiller-Texte).
- **Steam** verlangt bei einem späteren Verkauf eine Offenlegung von KI-Inhalten **[UNVERIFIZIERT, nicht geprüft]**.
- **Empfehlung:** Keine KI-generierten 3D-Modelle oder Texturen aus fremden Quellen übernehmen. In `CREDITS.md` eine Spalte „KI-generiert ja/nein“ führen.

### 3.4 Sonstiges

- **CC BY-SA, GPL** nicht mit Fab-Content mischen (§1.1).
- **Lizenzänderungen:** Autoren können die Lizenz auf Sketchfab nachträglich ändern oder Downloads abschalten (ob das bei Borchia V8 VC passiert ist, ist **[UNVERIFIZIERT]**; heute steht dort Editorial, nicht downloadbar). Die Lizenz zum Download-Zeitpunkt gilt (CC-Lizenzen sind unwiderruflich, CC BY 4.0 §2(a)(1)), muss aber **belegbar** sein. ⇒ Beim Download Seite als PDF/Screenshot mit Datum in `research/license-evidence/` ablegen (Git LFS).
- **Fab-Assets nie öffentlich** (Repo, Mods, Level-Editor-Export).
- **Freesound:** NC- und Sampling+-Sounds sehen in Suchergebnissen gleich aus. Immer die Lizenz der *Einzelseite* prüfen.

---

## 4. Empfohlenes Vorgehen

1. **M1:** UE Vehicle Template (SportsCar) + Poly Haven `asphalt_track` + ein Pure-Sky-HDRI + GiocoSound-Set (CC0). Alles ohne Attributionspflicht.
2. **M2:** Vintage Racing Car, Gourami, V12 Goblin als die 3 Autos prüfen (Topologie, UVs, Interieur). City Sample Vehicles als KI-Füller/Test.
3. Vor jedem Import: Lizenz-Screenshot ablegen, Zeile in `CREDITS.md` schreiben, Logo-/Formcheck dokumentieren.
4. Entscheidungen offen für den Projektinhaber: (a) Sonniss-Lizenz zulassen? (b) Bezahlte Fab-Autos (Standard License) für v1.0 zulassen? (c) engine-sim als Soundquelle?

---

## 5. Vorlage: `CREDITS.md`

```markdown
# APEXLINE – Credits

APEXLINE is a free, non-commercial hobby project. All vehicles, brands, tracks and
names in this game are fictional. Any resemblance to real products is unintended.

## Engine
- Made with Unreal® Engine. Unreal® is a trademark or registered trademark of
  Epic Games, Inc. in the United States of America and elsewhere.

## 3D models (vehicles)
| Asset (in game) | Original title | Author | Source | License | Changes |
|---|---|---|---|---|---|
| <Ingame-Name> | "Vintage Racing Car" | SpatialNeglect | https://sketchfab.com/3d-models/0b4cd62e8620486f9b62c32d87d3ed0e | CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/) | retopology, new materials, renamed |
| <Ingame-Name> | "Gourami - Luxury Saloon" | Fishboe | https://sketchfab.com/3d-models/bf29aca039c447ab913608e3f6818875 | CC BY 4.0 | … |
| <Ingame-Name> | "Fictional supercar - V12 Goblin" | Olli Teittinen (ollitei) | https://sketchfab.com/3d-models/0a20e49ad5774d778567cb5c3f345786 | CC BY 4.0 | … |

## 3D models (environment)
| Asset | Author | Source | License | Changes |
|---|---|---|---|---|
| "Race Track Assets Pack" | Cherk | https://sketchfab.com/3d-models/4a863e7005d647adb2f72775cf41e808 | CC BY 4.0 | … |

## Textures, materials, HDRIs (CC0 – credit not required, given with thanks)
- Poly Haven (https://polyhaven.com), CC0: asphalt_track (Dimitrios Savva), …,
  kloofendal_48d_partly_cloudy_puresky (Greg Zaal, Jarod Guest), …
- ambientCG (https://ambientcg.com), CC0: Road007, Asphalt033, …
  "Created using <asset> from ambientCG.com, licensed under the Creative Commons CC0 1.0 Universal License."

## Fab content (Fab Standard License – credit not required)
- City Sample Vehicles – Epic Games – https://www.fab.com/listings/2909157b-ddfa-4cef-a925-69dc2467021f
- <Megascans asset> – Quixel/Epic Games – <Fab URL>

## Sounds
| Sound (title) | Author | Source | License | Changes |
|---|---|---|---|---|
| "FR-S BRZ GT-86 Vortech Supercharged Dyno Run" | dezoris | https://freesound.org/people/dezoris/sounds/191194/ | CC BY 4.0 | sliced into RPM grains, EQ |
| "Backfire" | JustInvoke | https://freesound.org/people/JustInvoke/sounds/446101/ | CC BY 4.0 | … |
| "SFX_Car_Engine_Outside_RPMHigh.wav" | GiocoSound | https://freesound.org/people/GiocoSound/sounds/401551/ | CC0 | … |

## Music
| Track | Artist | Source | License |
|---|---|---|---|

## License texts
- CC BY 4.0: https://creativecommons.org/licenses/by/4.0/legalcode
- CC BY 3.0: https://creativecommons.org/licenses/by/3.0/legalcode
- CC0 1.0: https://creativecommons.org/publicdomain/zero/1.0/legalcode

---
<!-- Interne Spalten (nicht im Spiel anzeigen, nur im Repo pflegen):
| Asset | Download-Datum | Lizenz-Beleg (Pfad zu Screenshot/PDF) | KI-generiert? | Logo-/Formcheck durch/Datum |
-->
```

Hinweise zur Vorlage:
- CC-BY verlangt einen **Hinweis auf Änderungen** („Changes“-Spalte) und **Link zur Lizenz** (CC BY 4.0 §3(a)).
- Im Spiel: Credits-Screen im Hauptmenü mit denselben Einträgen. Die Datei `CREDITS.md` (bzw. `.txt`) zusätzlich ins Build-Verzeichnis kopieren.
- Fab Standard und CC0 verlangen keine Nennung. Sie trotzdem aufzuführen schadet nicht und hilft bei der Nachverfolgung.
