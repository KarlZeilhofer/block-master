# SPECS – Qt5 Kalender-Applikation

## 1. Überblick
- Desktop-Kalender für Linux, entwickelt mit Qt5 (Widgets, kein QML).
- Ziel: extrem flüssige, tastaturgetriebene Bedienung mit optionaler Maussteuerung.
- Fokus auf Tages-/Wochenplanung mit integrierter TODO-Liste, die per Drag-and-Drop in Termine überführt wird.
- Datenhaltung zunächst lokal (SQLite/JSON), perspektivisch Nextcloud-Synchronisation via CalDAV/CardDAV.

## 2. Benutzergruppen & Use Cases
1. Power-User, die viele Termine koordinieren und Tastenkürzel bevorzugen.
2. Projektteams, die TODOs zeitlich verplanen möchten.
3. Anwender, die zwischen Tages-, Wochen- und Monatsansicht wechseln müssen.

Hauptaufgaben:
- Termine erstellen, verschieben, kopieren, schnell bearbeiten.
- TODOs priorisieren und in Kalender-Zeitslots ziehen.
- Ansicht zoomen (horizontal = Date-Range, vertikal = Tageszeit-Detail).
- Schnelles Navigieren via Cursor-Tasten (inkl. PageUp/Down für größere Sprünge).

## 3. Zielplattform
- Linux Desktop, Qt Creator IDE, CMake-Projekt.
- Qt 5.15 LTS Bibliothek.
- Mindestauflösung 1366×768; optimiert für HiDPI über Qt HighDpiScaling.

## 4. UI-Konzept
```
┌────────────────────┬────────────────────────────────────────────┐
│ TODO-Panel (links) │ Hauptkalenderansicht (rechts)              │
│ - Filter            │ - Kopfzeile (Datum, Navigation)             │
│ - Gruppierung       │ - Scroll-/Zoom-Bereich für Termine          │
│ - Drag & Drop       │ - Inline-Editor / Detail-Panel (optional)   │
└────────────────────┴────────────────────────────────────────────┘
```

### 4.1 Paneelaufteilung
- **TODO-Liste**: anpassbare Breite, collapsible, enthält Filter (Status, Tag, Priorität). Elemente verschwinden nach erfolgreichem Drop in den Kalender (oder werden als „geplant“ markiert).
- **Kalenderbereich**: Tabs oder Umschalter für Ansichten (Tag/Woche/Arbeitswoche/Monat/Agenda). Separate Zoomslider bzw. Gesten (Ctrl+Scroll = horizontaler Zoom, Shift+Scroll = vertikal).

### 4.2 Navigationskopf
- Links/Rechts-Pfeile: Tag/Woche vor/zurück (abhängig von Ansicht).
- Heute-Button, Datumsauswahl (Popup-Kalender).
- Schnellfilter für Arbeitszeiten, ganztägige Termine, Ressourcen (optional).

## 5. Ansichten & Zoom
- **Tagesansicht**: Zeitachse vertikal (00–24h), Spalten pro Ressource/Kalender; vertikaler Zoom für Dichte.
- **Wochen-/Arbeitswochenansicht**: 7 bzw. 5 Spalten, horizontaler Zoom ändert Anzahl sichtbarer Tage (3–14).
- **Monatsansicht**: Raster, horizontales Zoomen blendet zusätzliche Wochen (z. B. 2 vs. 6).
- **Agenda/Listenansicht**: chronologische Tabelle mit Fokus auf Tastaturbedienung.
- **Standardstart**: Beim Programmstart sowie nach `T` (Heute) zeigt der Kalender immer exakt 9 Tage, beginnend an einem Montag, damit stets auch Montag und Dienstag der Folgewoche sichtbar bleiben; der linke Tag ist generell auf Wochenanfang ausgerichtet. Die Spaltenbreiten passen sich automatisch an die Fensterbreite an, sodass diese 9 Tage (und alle anderen Zoomstufen) den vorhandenen Platz vollständig ausnutzen.
- **Sticky Header**: Die Datums-/Titelzeile bleibt beim vertikalen Scrollen sichtbar und stellt jedes Datum zweizeilig in drei Blöcken dar (links großer fett gesetzter Tag, rechts daneben Wochentag sowie Monat+Jahr). Sonntage werden farblich hervorgehoben (rote Schrift/Hinterlegung), der aktuelle Tag erhält leuchtend grüne 3‑px‑Rasterlinien, während Monatsgrenzen mit identisch breiten schwarzen Linien markiert werden – so bleibt die zeitliche Orientierung ohne Scrollen erhalten.
- **Interaktion bei Drag & Drop**: Während ein Objekt per Drag bewegt wird (inkl. TODO → Kalender, Event-Verschiebung, Ghost-Placement) bleiben horizontale Scrollgesten vollständig nutzbar: das horizontale Mausrad springt weiterhin tageweise, Pfeiltasten lösen wie gewohnt die Navigation im MainWindow aus, und die Ansicht reagiert identisch wie ohne aktive Drag-Aktion.
- Zoominteraktion: Mausrad + Modifier, Touchpad-Pinch, dedizierte Tastenkürzel (`Ctrl++`, `Ctrl+-`, `Ctrl+Shift++` für vertikal). Standard-Scroll ohne Modifier bewegt die Ansicht vertikal in 15-Minuten-Schritten; horizontale Scrollleisten gibt es nicht, stattdessen scrollt das horizontale Mausrad exakt einen Tag vor/zurück und löst damit eine „freischwebende“ Darstellung (nicht mehr zwangsweise Montag) aus. `Ctrl+Scroll` (oder die Toolbar-Buttons) ändern die Anzahl sichtbarer Tage zwischen 1 und 31 und sorgen für horizontales Zoomen ohne Scrollbar, `Shift+Scroll` zoomt vertikal.
- Anzeige aktualisiert sich smooth (Scroll-Physik, Lazy Rendering bei >500 Events).

## 6. Editing-Flows
### 6.1 Direktes Editieren
- **Select-first-Prinzip**: Single-Click markiert ein Event, löst aber keine Editieransicht aus; erst explizite Aktionen (Shortcut, Button, Kontext) starten das Editing.
- Shift/Strg-Klick erweitert/separiert Mehrfachauswahl; `Esc` hebt die Auswahl auf, ohne Panels anzuzeigen.
- Direkte Daueranpassung über Drag an oberen/unteren Griffpunkten, Inline-Editor wird dabei nur auf Wunsch (Shortcut `E`) eingeblendet.
- Keyboard-Editing: `Enter` öffnet den Inline-Editor, `Tab` springt zwischen Feldern, `Alt+Enter` speichert und bleibt im Editor.

### 6.2 Erweitertes Editieren
- Kontextsensitives Detail-Panel (Dock rechts) zeigt eine **Preview-Karte** mit den wichtigsten Feldern und einer Shortcut-Leiste (z. B. `⏎ Edit`, `⌘C Copy`, `Del Löschen`). Dieses Panel öffnet sich nur, wenn explizit `Space` oder `I` gedrückt wird bzw. der Nutzer auf das Info-Icon klickt.
- Vollbild-Dialog für komplexe Felder (Beschreibung, Teilnehmer, Erinnerungen, Kategorien, Wiederholungen) wird über `Ctrl+Enter` oder Toolbar „Details…“ ausgelöst.
- Validation erst bei Speichern, Auto-Speichern nach Fokusverlust; Preview aktualisiert sich live.

### 6.3 Drag & Drop
- TODO aus linker Liste auf Zeit-Slot ziehen → Termin erzeugen, TODO verschwindet (oder wechselt in Status „geplant“).
- Zwischen Kalenderansichten Drag & Drop unterstützen (z. B. Tag → Woche).
- Drag-Klonen via `Ctrl`: kopiert Termin.
- Snap-Regeln: Raster 5 Minuten, Halten von `Alt` erlaubt freie Positionierung.
- Während des Draggens zeigt ein halbtransparentes Ghost-Objekt die erwartete Zielposition (am gleichen Raster), damit Nutzer das Ergebnis sehen, bevor sie loslassen.
- Der Inline-Editor bleibt während Drag-Vorgängen geschlossen; Feedback erfolgt ausschließlich über das Ghost-Overlay und Status-Bar-Botschaften.

### 6.4 Interaktionsworkflow & Shortcuts
- **Select**: Klick oder Pfeiltasten; Status-Bar blendet eine Mini-Legende ein („⏎ Edit | E Inline | Ctrl+D Duplicate | Del löschen“). Die Legende ist dezent (grau) und verschwindet nach 2 s ohne Interaktion.
- **Copy/Paste**: `Ctrl+C` kopiert ausgewählte Events in die Zwischenablage (inkl. Dauer), `Ctrl+V` fügt am Cursor/Zeitpunkt ein; Info-Legende zeigt diese Kürzel beim ersten Anwählen eines Events an.
- **Drag'n'Drop**: Ziehen bewegt Termine, `Ctrl+Drag` erstellt Klone, `Shift+Drag` beschränkt Bewegungen auf die Zeitachse (kein Datumssprung).
- **Delete**: `Del` / `Backspace` löscht mit Undo-Hinweis in der Status-Bar.
- **Edit**: `E` öffnet Inline-Editor am ausgewählten Event; `Ctrl+E` bzw. Toolbar öffnet Detail-Dialog, ohne automatische Anzeige beim Verschieben.
- **Details-Preview**: `Space` oder Klick auf das Info-Symbol blendet das Preview-Panel ein; erneut `Space` schließt es.
- **Paste-Kontext**: Beim Einfügen zeigt ein kontextuelles Toast („Eingefügt um 10:15 – Esc rückgängig“) die Aktion. Alle HUD/Legenden-Einblendungen sind im Layout vorgesehen (dezent, keine Modalität), damit Nutzer ohne Handbuch lernen können.

## 7. Tastaturbedienung
- Navigation:
  - `←/→`: verschieben den sichtbaren Zeitraum immer wochenweise (±7 Tage); befindet sich die Ansicht nicht auf einem Montag, richtet der erste Tastendruck die Darstellung zunächst auf den nächstgelegenen Wochenanfang aus, bevor weitere Sprünge erfolgen.
  - `↑/↓`: vertikales Scrollen (30 Minuten pro Schritt), bei Monatsansicht wechselt Woche.
  - `PgUp/PgDn`: größere Sprünge (Monat/Quartal).
  - `Home/End`: Anfang/Ende des sichtbaren Zeitraums.
  - `T`: springt zu Heute.
- Auswahl & Bearbeitung:
  - `Enter`: Inline-Edit des selektierten Items.
  - `Space`: Toggle Auswahl im TODO-Panel.
  - `Ctrl+D`: Duplizieren.
  - `Del`: Löschen (mit Undo).
- Zoom:
  - `Ctrl++` / `Ctrl+-`: horizontaler Zoom.
  - `Ctrl+Shift++` / `Ctrl+Shift+-`: vertikaler Zoom.
- TODO-Interaktion:
  - `Ctrl+N`: neues TODO.
  - `Ctrl+Shift+N`: neuer Termin an aktueller Cursorposition.

Alle Shortcuts konfigurierbar (QSettings).

## 8. Leistungsanforderungen
- Ziel: <16 ms Renderzeit pro Frame bei 500 sichtbaren Terminen.
- Virtualisierung der Items (nur sichtbare Widgets erstellen).
- Asynchrone Datenabfragen, UI-Thread nicht blockieren.
- Undo/Redo-Stack mit begrenzter Historie.
- Profiling-Hooks (Qt Creator Analyzer, Perfetto optional).

## 9. Datenhaltung & Sync (Platzhalter)
- Abstraktes Repository-Interface (z. B. `EventRepository`, `TodoRepository`).
- Lokaler Speicher (SQLite) mit Migrationen.
- Sync-Adapter (später): Nextcloud (CalDAV, Tasks), Konfliktauflösung optional.
- Offline-First, UI reagiert sofort, Sync asynchron im Hintergrund.

## 10. Erweiterbarkeit
- Plugin-Interface für zusätzliche Panels (z. B. Ressourcen).
- Themingsupport (Light/Dark) via Qt Stylesheet.
- Einstellungen persistieren via QSettings (Layout, Zoom, Shortcuts, Sync-Intervalle).

## 11. Qualitätssicherung
- Automatisierte Unit-Tests (Qt Test) für Datenmodelle und Presenter.
- GUI-Tests mit Squish oder Testautomation Framework (optional).
- Performance-Benchmarks (QTest::qBenchmark) für Rendering/Scrollen.

## 12. 10-Schritte-Implementierungsplan
1. **Projektgerüst anlegen**: CMake-Projekt mit Qt Widgets, grundlegende Module (Core, UI, Data). Einrichten von ClangFormat, CI-Basis.
2. **Modell-Interfaces definieren**: Datenklassen für Events, TODOs; Repositories (Mock-Implementationen) + Undo/Redo-Grundlage.
3. **Main Window & Paneelstruktur**: Splitter mit TODO-Liste und Kalenderstub, Toolbar/Navigationskopf, Grundlayout responsive.
4. **TODO-Panel implementieren**: QListView/QTreeView mit Model-View, Filter/Sortierung, Keyboardsteuerung, Drag-Source.
5. **Kalender-Rendering-Basics**: Custom-Widget für Tages-/Wochenansicht mit Timeline, Scroll/Zoom-Logik, Selektion, Performanceoptimierung (Item-Culling).
6. **Terminbearbeitung (inline)**: Selection-Controller, Inline-Editor, Resize-Handles, Tastaturkürzel, Undo/Redo.
7. **Erweitertes Editieren + Dialoge**: Detaildialog, Validierungen, wiederkehrende Events, Reminder.
8. **Drag & Drop Integration**: TODO → Kalender handling, interne Event-Verschiebung/Klonen, Feedback (Ghost-Preview).
9. **Monats-/Agenda-Ansichten & Zoom-Synchronisation**: Zusätzliche Views, gemeinsame Navigator-Logik, Shortcut-Management.
10. **Persistenz & Sync-Schnittstelle**: SQLite-Layer, abstrakte Sync-API (Nextcloud-Placeholder), Einstellungen/Preferences, Tests & Performanceprofiling.

## 13. Offene Punkte
- Genaues Datenformat für Nextcloud-Sync.
- Ressourcen-/Kalenderverwaltung (Mehrere Kalender?).
- Freigabe/Teamfunktionen.

Diese Spezifikation dient als Grundlage für schrittweise KI-gestützte Implementierung in Qt Creator.
