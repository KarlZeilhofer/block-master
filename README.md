# Block Master

Block Master ist eine auf Qt 5.15 basierende Desktop-Anwendung für Linux, die die Planung von Tagen und Wochen mit einer dreigeteilten TODO-Verwaltung kombiniert. Die Oberfläche ist komplett auf Tastaturbedienung und schnelles Drag-and-Drop ausgelegt, zeigt standardmäßig neun Tage ab einem Montag und blendet wichtige Statusinformationen in einer kompakten Statusleiste ein. Die Anwendung wird unter der GPL‑2.0 veröffentlicht und nutzt ein eigenes App-Icon aus `resources/block-master-icon.svg`.

## Highlights

### Kalenderansicht
- Standardansicht mit neun Tagen, immer ab Wochenbeginn, beliebiger Zoom von 1–31 Tagen.
- Sticky Header mit dynamisch verkürzten Datums-Labels, separater Monatsleiste bei schmalen Spalten, Sonntage rot, Monatsgrenzen schwarz und der aktuelle Tag mit grünen 3‑px-Rahmen.
- Horizontales Scrollen erfolgt in Schritten von ca. 5 % der Fensterbreite, abhängig vom Zoom (1/1, 1/2, 1/3 oder 1/4 Tag). Der rechte Tag bleibt sichtbar, auch wenn er nur teilweise im Viewport liegt. Cursor-Tasten verschieben weiterhin ganze Wochen/Tage.
- Vertikaler Scrollbalken wird nicht angezeigt, trotzdem bleibt Scrollen via Mausrad/Touchpad identisch.
- Eine rote Uhrzeitlinie liegt über allen Events und zeigt live die aktuelle Zeit.

### TODO-Panel
- Drei Listen („Offen“, „In Arbeit“, „Erledigt“) in einem vertikalen Splitter, Größen werden per `QSettings` gemerkt; erledigte Einträge sind grau dargestellt.
- Globale Suche (`Strg+F`) filtert alle Listen sowie Events gleichzeitig; Treffer bleiben farbig, andere Inhalte werden ausgegraut.
- TODOs zeigen ihre Dauer in Klammern. Plaintext-Einfüge- und Kopiermodus erlaubt Dauerangaben („2h“, „30min“) sowie strukturierte Beschreibungen inkl. `Ort:`-Zeilen. `Alt+Enter` bestätigt Dialoge.
- `Del` löscht sowohl ausgewählte TODOs als auch Events; die Liste selbst fängt den Shortcut ab und meldet Löschwünsche an das Hauptfenster.

### Drag & Drop und Ghosts
- TODO → Kalender erzeugt Termine mit korrekter Dauer; Events können zurück in eine TODO-Liste gezogen werden. Strg+Drag erstellt Kopien anstatt TODOs zu entfernen.
- Interne Event-Drags, Resize-Griffe und neue Drag-Erstellungen bleiben beim Scrollen aktiv. Ghost-Objekte aktualisieren sich auch dann, wenn nur horizontal oder vertikal gescrollt wird.
- Neue Events werden durch Aufziehen in leeren Bereichen erstellt. Ghost-Overlays zeigen Startzeit fett, Eventtitel, Wochentag und passen sich beim Scrollen an.

### Inline-Editor & Details
- Doppelklick oder `E` öffnet den Inline-Editor. Titel, Ort und Beschreibung sind editierbar; Start-/Endzeit werden read-only angezeigt und sind aus der Tab-Reihenfolge entfernt, damit sie ausschließlich grafisch verändert werden.
- Resize-Handles lassen sich auch außerhalb des Event-Körpers treffen. Ein ausgewählter Termin ändert seine Dauer nur, wenn obere oder untere Griffe aktiv sind.
- Detail-Dialog (`Ctrl+Enter`) bleibt für komplexere Felder verfügbar. Die Statusleiste zeigt dauerhaft wichtige Shortcuts statt Cursor-Koordinaten.

### Tastenkürzel (Auswahl)
- Navigation: `←/→` (wochenweise), `T` (Heute), `Ctrl++/−` (horizontaler Zoom), `Ctrl+Shift++/−` (vertikaler Zoom), `Shift`+Mausrad (Zeit-Zoom).
- TODO/Events: `Ctrl+N` neues TODO (übernimmt Suchtext), `Ctrl+Shift+N` neuer Termin, `Ctrl+D` dupliziert Event, `Ctrl+C`/`Ctrl+V` kopiert/fügt Termine an der Ghost-Position ein, `Del` löscht Auswahl.
- Editor: `Enter` speichert, `Alt+Enter` bestätigt Dialoge, `Esc` beendet Inline-Editor bzw. Abbrechen im Dialog.

## Installation & Build

### Voraussetzungen
- Linux mit CMake ≥ 3.16
- Qt 5.15 (Widgets, Test)
- g++ oder clang++ mit C++17-Support

### Konfiguration & Kompilierung
```bash
cmake -S . -B build
cmake --build build
```

### Ausführen & Testen
```bash
./build/block-master
ctest --test-dir build        # optional: führt UndoStack-, TodoRepository-, TodoListModel- und ScheduleViewModel-Tests aus
```

## Debian-Paket erstellen
Das Projekt besitzt bereits ein `install()`-Target, das `block-master` nach `bin/` kopiert. Für ein vollständiges `.deb` werden zusätzlich benötigt:
1. **Packaging-Metadaten**: `debian/control`, `debian/changelog`, `debian/rules` oder alternativ eine `CPackConfig.cmake` mit `CPACK_GENERATOR DEB`.
2. **Desktop-Integration**: `.desktop`-Datei in `/usr/share/applications/`, AppStream-Metadaten (`.metainfo.xml`) und das App-Icon (`resources/block-master-icon.png/svg`) in die entsprechenden `hicolor`-Verzeichnisse.
3. **Dokumentation & Lizenz**: `README.md` und `LICENSE` nach `/usr/share/doc/block-master/`.
4. **Abhängigkeiten**: Qt5-Laufzeitpakete (z. B. `qtbase5-dev`, `qttools5-dev-tools`). Diese gehören in `Depends` bzw. `Build-Depends`.
5. **Optional**: Manpages oder FAQ-Dateien für Support.

Ist das vorbereitet, kann entweder `cpack -G DEB` oder das klassische `dpkg-buildpackage` verwendet werden.

## Daten & Einstellungen
- Datenhaltung erfolgt in `default.ics` (Kalender + TODOs). Der `FileCalendarStorage` kümmert sich um Persistenz ohne Dummy-Daten.
- `QSettings` speichert Organisation `KarlZeilhofer`, Domain `zeilhofer.co.at` sowie u. a. sichtbare Tage, Scrollposition, Zoom, Splittergrößen und aktive Shortcuts.
- Drag in die TODO-Liste entfernt den Zeitbezug, behält aber die Dauer, damit erneute Platzierung eine korrekt skalierte Ghost-Vorschau zeigt.

## Ressourcen & Lizenz
- Ressourcenbündel: `resources/block-master.qrc` (SVG + PNG). Das Icon wird beim Start sowohl dem `QApplication` als auch dem Hauptfenster gesetzt.
- Lizenz: GNU General Public License Version 2 (siehe `LICENSE`). Beiträge müssen GPL‑2.0-kompatibel sein.

## Weiterführende Dokumente
- `SPECS.md`: UI- und Bedienkonzept im Detail.
- `Issues.txt`: Enthält offene Punkte, WIP und die Liste der bereits erledigten Features.

Fragen zu Umsetzung, Packaging oder neuen Features können direkt über die Issues geführt werden.
