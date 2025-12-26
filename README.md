# Block Master

Block Master ist eine auf Qt 5 basierende Desktop-Anwendung für Linux, die Tages- und Wochenplanung mit einer integrierten TODO-Verwaltung kombiniert. Der Fokus liegt auf flüssiger Bedienung mit Tastatur, präzisem Drag-and-Drop und einer klaren Darstellung von 9 Tagen zur gleichen Zeit – ideal für Power-User, die viele Termine organisieren und Aufgaben flexibel in Zeitblöcke ziehen möchten.

## Funktionsübersicht
- **Kalender-Board mit 9-Tage-Standardansicht**: Startet immer montags, beliebiger horizontaler Scroll um einzelne Tage, Sticky Header mit dynamisch verkürzten Datumslabels, getrennte Monatsleiste bei schmalen Spalten, Sonntage rot, heutiger Tag mit grünem Raster und aktueller Uhrzeitlinie.
- **TODO-Panel mit drei Listen**: Offen/In Arbeit/Erledigt, graue Darstellung für erledigte Aufgaben, Höhenverhältnis per Splitter einstellbar und persistent in `QSettings`. Globale Suchleiste filtert alle Listen gleichzeitig.
- **Drag & Drop überall**: TODOs lassen sich in den Kalender ziehen, Events wieder zurück in die TODO-Liste (inkl. Dauerübernahme). Während eines Drags bleiben horizontale/vertikale Scroll-Shortcuts aktiv, Ghost-Objekte passen sich beim Scrollen an.
- **Inline- und Detailbearbeitung**: Doppelklick oder `E` öffnet den Inline-Editor, Drag über die Zeitachse erzeugt neue Events, Ghost-Overlays zeigen Startzeit fett, kurze Events verzichten auf Endzeiten. Tooltips liefern alle Details im Klartext.
- **Überlappungen & Layout**: Überlappende Events werden intelligent verteilt, Hover hebt einzelne Termine hervor, Event-Karten haben 8 px-Ecken und schneiden beim Überschreiten von 0 Uhr sauber ab. Uhrzeiten stehen am linken Rand zweizeilig.
- **Suche & Clipboards**: Suchfeld beeinflusst auch Events (Treffer bleiben farbig, andere werden grau). Spezieller „Plaintext-Einfügen“-Modus erzeugt TODOs zeilenweise, inklusive Dauererkennung und Orte. `Strg+C`/`Strg+V`/`Strg+D` arbeiten konsistent und platzieren neue Elemente zentriert.
- **Undo/Redo & Persistenz**: Undo-Stack für riskante Aktionen (z. B. Plaintext-Paste). Daten werden in `default.ics` gespeichert; Dummy-Daten entfallen.
- **Branding & Oberfläche**: Fenster- und App-Icon stammen aus `resources/block-master-icon.*`, das Hauptfenster zeigt `Block Master 25.0.0` als Titel. Einstellungen liegen unter Organisation „KarlZeilhofer“, Domain `zeilhofer.co.at`.

## Tastaturbedienung (Auszug)
- Navigation: `←/→` (wochenweise), `↑/↓` (30 min Schritte), `T` (Heute), `Strg++/-` horizontaler Zoom, `Strg+Shift++/-` vertikaler Zoom.
- TODO/Events: `Strg+F` Suchfeld, `Strg+N` neues TODO (übernimmt aktuellen Suchtext), `Alt+Enter` bestätigt Dialoge, `Del` löscht Auswahl mit Undo-Hinweis, `Strg+D` dupliziert inkl. Ghost.
- Editor-Steuerung: `Enter`/`Alt+Enter` speichern Inline-Editor, `Esc` speichert und schließt, Handles funktionieren auch außerhalb des Event-Körpers für präzises Resizing.

## Installation & Build
1. **Voraussetzungen**  
   - Linux, CMake ≥ 3.16  
   - Qt 5.15 (Module `Widgets`, `Test`)  
   - g++ mit C++17-Support  
2. **Projekt konfigurieren und bauen**
   ```bash
   cmake -S . -B build
   cmake --build build
   ```
3. **Ausführen**  
   ```
   ./build/block-master
   ```
4. **Tests**  
   - `ctest --test-dir build` führt alle verfügbaren Qt-Testtargets aus (`UndoStack`, `TodoRepository`, `TodoListModel`, `ScheduleViewModel`).

### Hinweise zu .deb-Paketen
Das Projekt bringt bereits eine `install()`-Regel für das Binary mit. Für ein vollständiges Debian-Paket sollten zusätzlich installiert werden:
- `LICENSE` nach `/usr/share/doc/block-master/copyright`.
- Icon-Dateien in das passende `hicolor`-Verzeichnis (z. B. `/usr/share/icons/hicolor/128x128/apps/block-master.png`).
- Eine `.desktop`-Datei und optional AppStream-Metadaten nach `/usr/share/applications` bzw. `/usr/share/metainfo`.
Anschließend kann entweder CPack (`CPackConfig.cmake` + `CPACK_GENERATOR DEB`) oder der klassische `debian/`-Ordner mit `dpkg-buildpackage` verwendet werden.

## Daten & Einstellungen
- Kalender- und TODO-Daten liegen in `default.ics`; Events werden beim Start erneut geladen, die UI merkt sich den zuletzt sichtbaren Zeitraum und die Splitter-Positionen über `QSettings`.
- TODOs besitzen explizite Dauern, die im Panel angezeigt werden (z. B. `Briefing (45m)`); Standardwert 60 min, falls nicht gesetzt.
- Drag in die TODO-Liste entfernt den Zeitbezug, behält aber die Dauer, damit erneutes Planen korrekt dimensionierte Ghost-Objekte erzeugt.

## Icon & Ressourcen
- `resources/block-master.qrc` bündelt SVG/PNG-Varianten des App-Icons, die automatisch in das Binary eingebettet werden.
- Das Hauptfenster sowie die Anwendung verwenden dasselbe Icon, sodass Desktop-Umgebungen (z. B. GNOME/KDE) das Branding übernehmen können.

## Lizenz
Block Master wird unter der **GNU General Public License Version 2** veröffentlicht. Der vollständige Text steht in `LICENSE`. Beiträge müssen kompatibel zur GPL‑2.0 (oder nach Wahl späteren Versionen) sein.
