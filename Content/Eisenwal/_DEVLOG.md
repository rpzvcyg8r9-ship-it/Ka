# EISENWAL – Devlog

> Umgebung: Code und Skripte entstehen in einer Cloud-Umgebung **ohne Unreal Editor**.
> Kompilieren, Play-in-Editor-Tests und Screenshots gehen dort nicht.
> Jeder Eintrag sagt deshalb ehrlich, was geschrieben wurde und was **beim ersten Öffnen geprüft werden muss**.

## So startest du das Projekt

1. Repo klonen und `Eisenwal.uproject` per Rechtsklick → *Generate Visual Studio project files* vorbereiten.
   (Engine-Version ist auf 5.5 gesetzt. Andere 5.x-Version: Rechtsklick → *Switch Unreal Engine version*.)
2. Projekt öffnen und die Frage nach dem Kompilieren mit **Ja** beantworten (oder in Visual Studio `EisenwalEditor` bauen).
3. Beim ersten Start legt `Content/Python/init_unreal.py` automatisch alles an:
   Ordner, Materialien, DataTables und das Level `L_Ozean_Main`.
   Später jederzeit über das Menü **Eisenwal → Projekt einrichten / aktualisieren**.
4. Play drücken.

---

## Phase 1 – Reset, Ordner, Level, Ozean, Himmel, Licht

**Gemacht**
- Neues UE5-C++-Projekt `Eisenwal` (Module, Targets, Config). Lumen, Virtual Shadow Maps, Nanite,
  Volumetric Fog und Physik-Substepping sind aktiv. `L_Ozean_Main` ist Default- und Editor-Startmap.
- **Ordner** `/Game/Eisenwal/{Maps,Blueprints,Materials,Meshes,Data,UI,FX,Audio,Input}` (per Setup-Skript).
- **Archiv:** Menü *Eisenwal → Alte Assets archivieren* verschiebt alles außerhalb von `/Game/Eisenwal`
  nach `/Game/_ALT_Archiv`. Es wird nichts gelöscht. Das Repo selbst war leer, dort gab es also nichts zu archivieren.
- **Wellen-Manager** `UEisenwalOceanSubsystem`: einzige Quelle für die Wasseroberfläche.
  - Grundseegang: 8 Gerstner-Wellen, gesteuert durch Beaufort + Windrichtung (`DT_Seegang`), weiche Übergänge.
  - Einzelwellen (Tsunami/Monsterwelle, gerade oder ringförmig) mit Vorlauf-Tal („Wasser zieht sich zurück“),
    Aufsteilung im Flachwasser und Strömung, die das Schiff mitreißt.
  - Strudel (Trichter + Spiralarme + Sog), Gasblasen-Zonen (weniger Auftrieb), Flachwasser um Inseln.
- **Ozean-Mesh** `AEisenwalOcean`: 5 Detailringe folgen der Kamera (4 m Gitter innen bis 96 km Horizont) und fragen
  dieselbe Funktion ab wie die Physik. Schiff und sichtbare Welle passen deshalb immer zusammen.
- **Himmel** `AEisenwalSky`: Sonne, Mond (mit Mondscheibe und Phase), Sky Atmosphere, Volumetric Clouds, Sky Light
  (Echtzeit-Capture), Höhennebel (volumetrisch), Post-Process-Farbstimmung, Sternenkuppel, Blitz-Effekt.
- **Materialien** (per Skript): `M_Ozean` (Tiefe/Kamm/Schaum über Vertex-Farben, prozedurale Riffel-Normale),
  `M_Sterne` (additiv, funkelnd).
- `AEisenwalGameMode` erzeugt Himmel und Ozean automatisch, falls sie im Level fehlen.

**Abweichung vom Plan (mit Grund)**
- Statt des *Water-Plugins* nutzt das Projekt ein eigenes Ozean-System. Tsunamis, Monsterwellen und Strudel
  müssen die Oberfläche **für Grafik und Physik gleichzeitig** verformen. Beim Water-Plugin gehen die
  Gerstner-Wellen nicht zur Laufzeit zu verbiegen, ohne dass Schiff und Welle auseinanderlaufen.

**Beim ersten Öffnen prüfen**
- Kompiliert alles? (ungetestet, ohne Engine geschrieben)
- Ist der Ozean von oben sichtbar? Wenn nur von unten: am Ozean-Actor `bFlipWinding` anhaken.
- Helligkeit von Sonne und Nacht (Auto-Exposure in `AEisenwalSky`) nach Geschmack anpassen.

**Offen / Platzhalter**
- Wolkenmaterial ist das Engine-Standardmaterial. Wolkendichte je Wetter braucht später ein eigenes `M_Wolken`.
- Übergänge zwischen den Detailringen können bei hohem Seegang minimal sichtbar sein.
