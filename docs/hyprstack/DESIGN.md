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

## Testy E2E

- `hyprtester/` (upstream): headless kompozytor + plugin testowy (symulacja kliknięć, klawiatury,
  dragów) + zapytania przez socket. Nowe testy stackingu dochodzą jako osobne pliki w
  `hyprtester/src/tests/stack/`.
- `scripts/e2e/hs-session`: ręczne/skryptowe sesje headless — `start`, `spawn`, `ctl`, `shot`
  (grim), opcjonalnie VNC (`--vnc PORT`, nasłuch na 127.0.0.1 → tunel SSH).
- Klienci testowi: `foot` (SSD), `gtk4-demo` (CSD), `xterm` (XWayland).
- Środowisko VM: brak sesji logind po SSH → `LIBSEAT_BACKEND=noop` (DRM otwierany bezpośrednio
  dla alokatora GBM), render przez llvmpipe.
