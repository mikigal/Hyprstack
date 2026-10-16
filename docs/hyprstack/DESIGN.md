# Hyprstack — projekt

Hyprstack to fork Hyprlanda dostosowany do pracy w trybie **stacking** (jak KDE/KWin),
zamiast tilingu. Shell (panel, taskbar, launcher, Alt+Tab UI) powstanie osobno w Quickshellu.

## Decyzje (zatwierdzone)

| Temat | Decyzja |
|---|---|
| Upstream | Soft fork. `main` = mirror `hyprwm/Hyprland`, praca na branchu `hyprstack`, regularny merge `upstream/main`. Nowy kod w osobnych plikach (`src/stack/...`), minimalne haki w plikach upstreamu. Tiling wyłączony/ukryty, nieusuwany. |
| Pulpity | Jeden pulpit. Każdy monitor ma jeden stały workspace; okna przesuwa się między monitorami. Mechanizm workspace'ów zostaje w kodzie (uśpiony), bez UI/bindów w domyślnej konfiguracji. |
| Dekoracje | SSD rysowane przez kompozytor (belka: tytuł, przyciski, drag, dwuklik). Negocjacja SSD przez `xdg-decoration` i `org_kde_kwin_server_decoration`. Klienci z CSD (GTK) — bez belki serwera. |
| Nazwa | Drop-in zamiennik Hyprlanda (binarka `Hyprland`, `hyprctl`, te same ścieżki i IPC) — minimalizuje konflikty przy merge'ach, Quickshell działa bez zmian. |
| Alt+Tab | UI w Quickshellu. Kompozytor dostarcza listę okien w kolejności MRU, eventy IPC i miniatury (toplevel-export). |
| Snapping | Jak KDE: krawędź L/P → połowa, róg → ćwiartka, góra → maximize; półprzezroczysty podgląd; przyciąganie do krawędzi okien/ekranu. |
| Skróty | Jak Windows: Win+←/→ snap, Win+↑ maximize, Win+↓ restore/minimize, Win+D pokaż pulpit, Win+Shift+←/→ okno na inny monitor, Alt+F4, Alt+Tab. Przesuwanie głównie belką; Alt+drag opcjonalnie w configu. |
| Fokus | Click to focus + raise. Najechanie myszą nie zmienia fokusu. |

## Analiza kodu upstreamu (0.56, `e2ad066c`)

- `src/layout/`: każdy workspace ma `CSpace` → `CAlgorithm` = `ITiledAlgorithm` + `IFloatingAlgorithm`.
  `CAlgorithm::addTarget` kieruje okno do tiled/floating wg `ITarget::floating()`.
  → Stacking = własny algorytm floating + wymuszenie `floating` dla wszystkich okien.
- Maximize jest realizowany jako tryb fullscreen `FSMODE_MAXIMIZED` (`src/managers/fullscreen`) —
  bez zachowania dekoracji/geometrii w sensie KDE.
- Minimize nie istnieje: żądania (xdg `set_minimized`, X11 iconic, wlr-foreign-toplevel) są tylko
  emitowane jako event (`Event::bus()->m_events.window.minimize`, IPC `minimized`).
- Z-order: `Desktop::CWindowState` (lista okien, `raise`/`lower`). Brak warstw keep-above/below,
  brak reguły "dialog nad rodzicem".
- Dekoracje: `src/render/decorations/IHyprWindowDecoration` z `onInputOnDeco` (wzorzec: groupbar).
- Config: wyłącznie Lua (`src/config/lua`).

## Architektura forka (warstwy)

1. **Silnik stackingu** (`src/stack/`)
   - `CStackingAlgorithm` (floating) — polityki rozmieszczania nowych okien, ograniczanie do
     obszaru roboczego, pamięć geometrii do przywracania.
   - Wszystkie okna floating; tiled algorithm = no-op; `togglefloating` bez efektu.
   - Prawdziwy maximize (pełny / pionowy / poziomy) z przywracaniem geometrii; stan `maximized`
     wysyłany klientowi; belka i obramowanie zostają.
   - Minimize: stan okna (niewidoczne, nieklikalne, nie dostaje fokusu), obsługa xdg/X11/
     foreign-toplevel, event IPC, przywracanie przy aktywacji.
   - Z-order: raise on click, warstwy keep-above / normal / keep-below, transient (dialogi)
     zawsze nad rodzicem.
   - Fokus: click-to-focus domyślnie, historia MRU (do Alt+Tab), focus stealing prevention.
2. **Interakcja**
   - Belka tytułowa SSD (`CHyprTitlebarDecoration`): tytuł, min/max/close, drag = move,
     dwuklik = maximize, theming w Lua.
   - Resize z krawędzi okna z poszerzonym obszarem chwytu i kursorami.
   - Quick-tile / edge snap (połówki, ćwiartki, maximize przy górnej krawędzi) z podglądem.
   - Przyciąganie do krawędzi ekranu/okien (istnieje `general:snap`).
   - "Pokaż pulpit".
3. **Integracja z shellem**: kompletne `wlr-foreign-toplevel` (minimize/maximize/activate/close/
   rectangle), `ext-foreign-toplevel-list`, rozszerzone eventy IPC, eksport miniatur
   (toplevel-export) dla Alt+Tab/taskbara.
4. **Domyślna konfiguracja** w stylu KDE (skróty, brak gapsów, layout tiling niedostępny).

## Plan faz

1. Rdzeń stackingu: wszystko floating, rozmieszczanie, z-order + raise on click, maximize, minimize (+ testy E2E).
2. Belki SSD + resize z krawędzi.
3. Quick-tile / snapping z podglądem + skróty Win+strzałki.
4. Integracja z shellem (foreign-toplevel, IPC, MRU, miniatury) + roboczy shell w Quickshellu do testów.
5. Domyślny config w stylu Windows, ukrycie tilingu.

### Zgodność z prawdziwymi aplikacjami (2026-10-16)

Przetestowane na VM: Firefox, Chromium, VS Code (Electron), Kate, Dolphin, LibreOffice, GIMP, gtk4-demo, foot, kitty, xterm.
Znalezione i naprawione:
- Klient jawnie proszący o client-side (Firefox przez KDE server-decoration, `foot -o csd.preferred=client`)
  dostawał wymuszone server-side i podwójną belkę — w trybie stacking jawna preferencja klienta wygrywa.
- Input dla okien z offsetem geometrii xdg: `windowSurfaceAt`, fallbacki w InputManager i
  `CWLSurface::getSurfaceBoxGlobal` (przytrzymany przycisk) liczyły współrzędne bez offsetu → kliknięcia
  trafiały w margines cieni (Firefox zaczynał resize zamiast przesuwania).
- Popupy okien CSD rysowane z podwójnie odjętym offsetem (menu przesunięte w lewo/górę).
- Blur pod marginesem cieni CSD dawał widoczny prostokąt — wyłączony dla takich powierzchni.
- `unset_maximized` od aplikacji był ignorowany (ścieżka fullscreen) — żądania maximize okien stacking
  idą prosto do `CWindowStack`.
- `hl.dsp.cursor.move` nie wysyłał `wl_pointer.frame` → GTK/Firefox wstrzymywały zdarzenia (testy).
- Dialogi mają w belce tylko "zamknij", okna o stałym rozmiarze bez "maksymalizuj".
- Upstreamowe okienka `hyprland-welcome` / `hyprland-update-screen` wyłączone w trybie stacking.
Uwagi: Firefox bez xdg-desktop-portal nie otwiera dialogów plików (to środowisko, nie kompozytor);
Firefox sam zapamiętuje i przywraca swój rozmiar.

## Regresje względem upstreamu

Porównanie (2026-10-16, chunked, te same warunki, `~/private/compare-suites.sh` na VM): 280 testów upstreamu.
Testy padające tylko na forku powtórzone 2× na obu binarkach — wszystkie flaky także na upstreamie
(timing kitty/llvmpipe na VM), poza `dwindleFloatClamp` (tiling; fork 1/6, upstream 4/5 przejść,
niewyjaśnione, ręczne odtworzenie daje identyczny wynik na obu binarkach). Tiling nie jest celem forka.

## Testy E2E

- `hyprtester/` (upstream): headless kompozytor + plugin testowy (symulacja kliknięć, klawiatury,
  dragów) + zapytania przez socket. Nowe testy stackingu dochodzą jako osobne pliki w
  `hyprtester/src/tests/stack/`.
- `scripts/e2e/hs-session`: ręczne/skryptowe sesje headless — `start`, `spawn`, `ctl`, `shot`
  (grim), opcjonalnie VNC (`--vnc PORT`, nasłuch na 127.0.0.1 → tunel SSH).
- Klienci testowi: `foot` (SSD), `gtk4-demo` (CSD), `xterm` (XWayland).
- Środowisko VM: brak sesji logind po SSH → `LIBSEAT_BACKEND=noop` (DRM otwierany bezpośrednio
  dla alokatora GBM), render przez llvmpipe.

## Stan implementacji

### Faza 1 — rdzeń stackingu (zrobione)

- Layout `stacking` (domyślny `general:layout`): `CStackingTiledAlgorithm` (wszystko oddaje do floating)
  + `CStackingFloatingAlgorithm` (rozmieszczanie: `stacking:placement` = smart/center/cascade/mouse,
  dialogi wyśrodkowane nad rodzicem, przycinanie do obszaru roboczego).
- `Stack::CWindowStack` (`src/stack/WindowStack.*`):
  - maximize pełny/pionowy/poziomy z przywracaniem geometrii, wiele zmaksymalizowanych okien naraz,
    prawdziwy stan `maximized` wysyłany klientom xdg, dopasowanie przy zmianie obszaru roboczego / monitora,
    ręczny move/resize kończy maximize;
  - minimize (okno ukryte, fokus wraca do poprzedniego wg MRU), żądania z xdg/X11/foreign-toplevel;
  - warstwy keep-above / normal / keep-below, dialogi (transient) zawsze nad rodzicem;
  - raise przy fokusie z klawiatury/dispatchera/aktywacji (`stacking:raise_on_focus`), hover nie wynosi.
- Dispatchery: `hl.dsp.window.minimize/maximize/keep_above/keep_below`; `fullscreen({mode="maximized"})`
  na workspace stacking robi prawdziwy maximize.
- IPC: `hyprctl clients` ma pola `minimized`, `maximized` (0/1=H/2=V/3=pełny), `stackLayer` (0/1/2);
  eventy socket2 `maximized>>ADDR,MODE`, `minimized>>ADDR,0|1`, `stacklayer>>ADDR,LAYER`.
- Testy: `hyprtester/src/tests/stack/` (grupa `stack`), uruchamianie: `STACK_ONLY=1 scripts/e2e/run-hyprtester`.

### Faza 2 — belki tytułowe i okna CSD (w toku)

- `Stack::CTitlebarDecoration` (`src/stack/decoration/`): doczepiana do każdego okna, widoczna tylko
  dla okien na workspace stacking, które wynegocjowały SSD (xdg-decoration / KDE) albo są X11 bez
  motif "no decorations". Tytuł, przyciski (`stacking:titlebar:buttons`, akcja przy puszczeniu jak
  w Windows), dwuklik = maximize, przeciąganie = move, środkowy = lower, prawy = event IPC
  `titlebarmenu>>ADDR,X,Y` (menu okna zrobi shell). Ramka obejmuje belkę i okno.
- Okna CSD (GTK, Chromium…): brak ramki/cienia/zaokrągleń kompozytora (priorytet `PRIORITY_LAYOUT`,
  reguły okien wygrywają; `stacking:decorate_csd` przywraca), prawdziwy stan maximized, brak
  wymuszonych stanów `tiled`, capability `MINIMIZE`.
- Upstream nie obsługiwał `xdg_surface.set_window_geometry` dla głównej powierzchni (komentarz
  "FIXME ... We always set MAXIMIZED anyways"). Dla okien stacking renderer rysuje całą powierzchnię
  z offsetem geometrii (cienie CSD poza ramką), extents okna obejmują ten nadmiar (damage).
- Rozmieszczanie przewiduje wymiary ramki i belki przed zmapowaniem (positioner pomija niezmapowane
  okna); pozycje X11 równe narożnikowi monitora traktowane jako "brak preferencji".

### Faza 3 — quick-tile i snap (zrobione)

- `Stack::CQuickTileController` (`src/stack/QuickTile.*`): przeciągnięcie okna do zewnętrznej krawędzi
  ekranu → połowa, róg (`stacking:quick_tile:corner_size`) → ćwiartka, góra → maximize
  (`top_action`). Krawędzie wspólne z innym monitorem nie reagują (przenoszenie okien między
  monitorami). Podgląd rysowany tuż przed przeciąganym oknem (nad innymi, pod nim), z blurem.
- Stan kafla w `CWindowStack` (bitmaska krawędzi), wspólna geometria przywracania z maximize,
  stany xdg `tiled_*` dla klienta, przeliczanie przy zmianie obszaru roboczego, przeciągnięcie
  kafla przywraca rozmiar.
- Dispatchery: `hl.dsp.window.tile({edges})`, `hl.dsp.window.snap({direction})` — semantyka Win+strzałki
  (←/→ połowa, ponownie → kolejny monitor, przeciwny kierunek przywraca; ↑ ćwiartka/maximize;
  ↓ ćwiartka/przywrócenie/minimize). IPC: pole `tiled`, event `tiled>>ADDR,EDGES`.

### Faza 4 — integracja z shellem (zrobione, poza oknem menu)

- Aktywacja zminimalizowanego okna z dowolnego źródła (foreign-toplevel `activate`, xdg-activation,
  `hl.dsp.focus`) przywraca je (hak w `CFocusState::fullWindowFocus`).
- `hl.dsp.show_desktop()` — jak Win+D: minimalizuje widoczne okna, ponownie przywraca (z kolejnością
  i fokusem); przywrócenie/otwarcie okna kończy tryb. Event `showdesktop>>0|1`.
- Dla shella (Quickshell): `ToplevelManager` (wlr-foreign-toplevel: minimize/maximize/activate/close,
  stany MINIMIZED/MAXIMIZED), `Hyprland.toplevels[].lastIpcObject.focusHistoryID` = kolejność MRU,
  `ScreencopyView` na miniatury (działa też dla zminimalizowanych — ostatnia klatka),
  `GlobalShortcut` do Alt+Tab, eventy `maximized`, `tiled`, `minimized`, `titlebarmenu`.
- Roboczy shell do testów: `scripts/e2e/dev-shell/` (taskbar + Alt+Tab z miniaturami),
  `hs-session start --shell --plugin`. Znany problem roboczego shella: pierwsze Alt+Tab po starcie
  czasem gubi puszczenie Alt (fokus klawiatury layer-surface).
- Menu okna: kompozytor wysyła `windowmenu>>ADDR,X,Y` (prawy klik na belce, `hl.dsp.window.menu()` /
  Alt+Spacja, `xdg_toplevel.show_window_menu`), ogłasza capability `WINDOW_MENU`; menu rysuje shell
  (roboczy: `scripts/e2e/dev-shell/WindowMenu.qml`).
- Pamięć geometrii per app-id (`src/stack/GeometryMemory.*`), plik w `$XDG_STATE_HOME/hyprstack/`.

### Faza 5 — domyślny config (zrobione)

- `example/hyprstack.lua` jest osadzany jako generowany domyślny config (zamiast `example/hyprland.lua`):
  layout `stacking`, brak gapsów, ramka 1 px, zaokrąglenia 8, resize z krawędzi, magnetyczne krawędzie,
  `follow_mouse = 2` (click-to-focus, wskaźnik nadal trafia do okna pod kursorem), `cursor:no_warps`,
  `focus_on_activate`. Skróty jak w Windows: Win+←/→/↑/↓ (snap), Win+Shift+←/→
  (`hl.dsp.window.move_to_monitor`), Win+D, Alt+F4, Alt+Tab (globalny skrót dla shella),
  Super+LPM/PPM przesuwanie/resize. Bez bindów workspace'ów.

### Haki w plikach upstreamu (sprawdzać przy merge'ach)

| Plik | Hak |
|---|---|
| `config/values/ConfigValues.cpp` | dołączenie `Stack::Config::getConfigValues()`, domyślny `general:layout = stacking` |
| `layout/supplementary/WorkspaceAlgoMatcher.*` | rejestracja algorytmów `stacking`, wybór algorytmu floating |
| `layout/algorithm/floating/default/DefaultFloatingAlgorithm.hpp` | `#pragma once`, `private` → `protected` |
| `layout/space/Space.cpp` | `toggleTargetFloating` = no-op na stacking |
| `desktop/view/window/Window.*` | `setFloatingInitial(true)` przy mapowaniu, flaga `m_minimized`, żądanie minimize |
| `desktop/state/WindowState.*` | `applyStackPolicy()` po każdej zmianie z-order |
| `managers/fullscreen/FullscreenController.cpp` | przechwycenie `FSMODE_MAXIMIZED` |
| `config/shared/actions/ConfigActions.cpp` | toggle maximize w `fullscreenWindow` |
| `config/lua/bindings/LuaBindingsDispatchers.cpp` | rejestracja dispatcherów stack |
| `desktop/view/window/WaylandBackend.cpp` | prawdziwy stan maximized, jawne żądania maximize |
| `protocols/XDGShell.cpp` | brak wymuszonego `maximized` przy mapowaniu w trybie stacking |
| `protocols/ForeignToplevelWlr.cpp` | minimize przez `Stack`, stany MAXIMIZED/MINIMIZED |
| `ipc/s1/Commands.cpp` | pola stack w `clients` |
| `hyprtester/src/main.cpp` | grupa testów `stack` |
| `desktop/view/window/WindowPresentation.cpp` | doczepienie `CTitlebarDecoration` |
| `desktop/view/window/Window.cpp` (`getFullWindowExtents`) | nadmiar powierzchni CSD w extents |
| `render/Renderer.cpp` (`renderWindow`) | offset geometrii xdg dla okien stacking |
| `protocols/XDGDecoration.*`, `protocols/ServerDecorationKDE.*` | zapytanie "czy okno ma SSD" |
| `protocols/XDGShell.*` | bez stanów `tiled` i z capability `MINIMIZE` w trybie stacking, `setTiledEdges()` |
| `desktop/view/window/WaylandBackend.*` | `setTiledEdges()` |
| `layout/supplementary/DragController.cpp` | drag-restore maximized/tiled, podgląd quick-tile |
| `managers/input/InputManager.cpp` | ramka resize-on-border obejmuje belkę |
| `desktop/state/FocusState.cpp` | fokus zminimalizowanego okna je przywraca |
| `config/lua/DefaultConfig.hpp` | osadzany `example/hyprstack.lua` |
| `desktop/state/ViewHitTester.cpp`, `desktop/view/WLSurface.cpp` | offset geometrii xdg w inpucie |
| `render/Renderer.cpp` (popupy, blur) | pozycja popupów okien stacking, brak blura pod cieniami CSD |
| `desktop/view/window/Window.cpp` (`onUpdateState`) | żądania maximize okien stacking |
| `config/shared/actions/ConfigActions.cpp` (`moveCursor`) | `wl_pointer.frame` po warpie |
| `managers/WelcomeManager.cpp` | brak ekranu powitalnego w trybie stacking |
