# Hyprstack — przekazanie pracy (dla Claude Code)

Ten plik jest punktem startowym dla sesji Claude Code na nowej maszynie. Czytaj razem z
[DESIGN.md](DESIGN.md) (decyzje, architektura, tabela haków w plikach upstreamu) i
[README.md](README.md) (funkcje, opcje, dispatchery, IPC).

## Kontekst i zasady

- Prywatny fork Hyprlanda (github.com/mikigal/Hyprstack) dostosowany do pracy **stacking** (jak KDE /
  Windows). Nigdy nie trafi do upstreamu — polityka AI i reguły PR z `AGENTS.md` / `CLAUDE.MD` w repo
  **nie obowiązują**. Licencja też nie jest tematem (prywatny fork).
- Użytkownik pisze po polsku. **Przed ważnymi decyzjami projektowymi pytaj**; drobne rzeczy rób sam.
  Push na branch `hyprstack` jest dozwolony bez pytania.
- Rzeczy tymczasowe / wrażliwe trzymaj poza repo (repo jest publiczne), np. w `~/private`.
- Shell (panel, taskbar, Alt+Tab UI, menu okna) będzie pisany osobno w Quickshellu. W repo jest tylko
  roboczy shell do testów: `scripts/e2e/dev-shell/`.

### Zatwierdzone decyzje (skrót, pełna tabela w DESIGN.md)

soft fork (`main` = mirror upstreamu, praca na `hyprstack`, nowy kod w `src/stack/`, minimalne haki) ·
brak wirtualnych pulpitów — jeden stały workspace na monitor (użytkownik ma 3 monitory i przesuwa okna
między nimi) · belki SSD rysowane przez kompozytor, klienci CSD bez belki · drop-in zamiennik (ta sama
binarka `Hyprland`, `hyprctl`, IPC) · Alt+Tab i menu okna rysuje shell (kompozytor daje eventy/MRU) ·
quick-tile jak KDE · skróty jak Windows · click-to-focus + raise.

## Maszyna (po `scripts/hyprstack/setup-arch.sh`)

- Repo: `~/work/Hyprstack` (branch `hyprstack`, remote `upstream` = hyprwm/Hyprland).
- `build-release/` (RelWithDebInfo) zainstalowany do `/usr/local` — to jest sesja, w której
  użytkownik pracuje (sesja „Hyprstack” w display managerze albo `start-hyprland --path
  /usr/local/bin/Hyprland` z tty).
- `build/` (Debug + `-DTESTS=true`, tylko z `--dev`) — używany przez `scripts/e2e/*` i hyprtester.
- Config użytkownika: `~/.config/hypr/hyprland.lua` (+ symlink `hyprlandd.lua`), wzorzec
  `example/hyprstack.lua` (wbudowany w binarkę jako domyślny config).
- Pakiet `hyprland` z repo Archa **nie jest** instalowany (konflikt nazw z `/usr/local`).

Po zmianie kodu: `cmake --build build-release && sudo cmake --install build-release`, potem
przelogowanie (`hyprctl dispatch 'hl.dsp.exit()'`). Uwaga: to restartuje sesję, w której działa
też terminal z Claude — uprzedź użytkownika, nie rób tego sam w trakcie pracy.

## Workflow

- Build dev: `cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTESTS=true -S . -B build && cmake --build build`
- Testy forka: `STACK_ONLY=1 scripts/e2e/run-hyprtester` (grupa `stack`, 37 testów, wszystkie
  przechodzą). Pojedyncze testy: nazwy jako argumenty.
- Ręczna/skryptowa sesja testowa: `scripts/e2e/hs-session start [--shell] [--plugin] [--vnc PORT]
  [--default-config] [--res WxH] [--outputs N]`, potem `spawn`, `exec`, `ctl` (hyprctl),
  `shot` (grim → PNG), `log`, `stop`. `hs-session env` wypisuje zmienne do ręcznego użycia.
- Testy dochodzą w `hyprtester/src/tests/stack/` (`helpers.*`: `enterStacking`, `spawnFoot`,
  `window`, `zOrder`, `workAreaOf`, `placeMonitorRightOf`…), grupa zarejestrowana w
  `hyprtester/src/main.cpp`.
- Synchronizacja z upstreamem: `scripts/hyprstack/sync-upstream.sh`, potem przejrzyj tabelę haków
  w DESIGN.md i uruchom testy.

### Tryby uruchamiania kompozytora testowego (`scripts/e2e/lib.sh`)

- **nested** — gdy ustawione `WAYLAND_DISPLAY` (terminal w sesji Hyprstack na prawdziwym sprzęcie):
  instancja testowa rysuje się w oknie, `AQ_DRM_DEVICES` wskazuje na nieistniejące urządzenie, żeby
  nie próbowała przejąć GPU. Wymaga prawdziwego GPU (render node). **Na nowej maszynie jeszcze
  niesprawdzone** — pierwsza rzecz do zweryfikowania (`scripts/e2e/hs-session start`, potem
  `hs-session ctl monitors` — monitory muszą mieć niezerowy rozmiar).
- **headless** — bez `WAYLAND_DISPLAY` i bez działającej sesji: `LIBSEAT_BACKEND=noop`, kompozytor sam
  otwiera DRM. Działa tylko, gdy żadna sesja nie trzyma GPU.
- Skrypty nigdy nie zabijają sesji uruchomionej przez `start-hyprland` (rozpoznawana po rodzicu);
  zabijają tylko własne zbłąkane instancje testowe.

## Pułapki (nauczone na VM)

- **Build Debug czyta `hyprlandd.lua`** (z „d”), nie `hyprland.lua` — stąd symlink. Brak symlinku =
  domyślny config i niedziałający `hl.on("hyprland.start", ...)`.
- Jedna karta DRM = jeden DRM master. Headless nie wystartuje, gdy działa sesja na ekranie
  (monitory 0x0 / „no allocator”).
- Na VM z llvmpipe tryb nested nie działa (`DRM_IOCTL_MODE_CREATE_DUMB: Permission denied` — software
  GBM chce dumb buffers na węźle karty należącym do sesji). Na prawdziwym GPU nie powinno to
  występować.
- **Nie używaj `pkill -f <wzorzec>`** — wzorzec jest w linii poleceń twojej własnej powłoki i ją zabija.
  Używaj `pgrep -x` / pidów.
- Duże aplikacje potrafią przy crashu zapchać dysk przez systemd-coredump — na VM ograniczone w
  `/etc/systemd/coredump.conf.d/limits.conf` (`ProcessSizeMax=1G`, `ExternalSizeMax=1G`,
  `MaxUse=4G`). Warto ustawić podobnie, jeśli się pojawi problem.
- Część upstreamowych testów hyprtestera była flaky na VM (timing kitty/llvmpipe). Porównuj z czystym
  buildem upstreamu (worktree `main` w osobnym katalogu) zanim uznasz coś za regresję forka.
- `dwindleFloatClamp` (tiling) — niewyjaśniona różnica przechodzenia fork vs upstream; tiling nie jest
  celem forka.
- Przeciąganie w testach: aktualizacje dragu są throttlowane do odświeżania monitora — między ruchami
  kursora potrzebny sleep. `hl.dsp.cursor.move` w forku wysyła `wl_pointer.frame` (GTK/Firefox bez
  tego wstrzymują zdarzenia).
- Testy wielomonitorowe: hyprtester ma tylko `HEADLESS-2`; testy tworzą `HEADLESS-STACK` i ustawiają
  pozycje jawnie (auto-right przestawia monitory).
- Okna CSD (GTK, Chromium, Firefox): powierzchnia jest przesunięta o offset `xdg window_geometry`
  (cienie) — input/render/popupy liczą to przez `Stack::mainSurfaceOrigin` / `surfaceOverflow`
  (`src/stack/decoration/DecorationPolicy.*`). Nowe miejsca liczące pozycję powierzchni muszą to
  uwzględniać.

## Stan (2026-10-16)

Fazy 1–5 z DESIGN.md zrobione: rdzeń stackingu (placement, z-order, warstwy, prawdziwy
maximize/minimize z animacją do przycisku taskbara), belki SSD + polityka CSD, quick-tile i Win+strzałki,
integracja z shellem (foreign-toplevel, MRU, eventy IPC, `windowmenu`), pamięć geometrii per aplikacja,
domyślny config w stylu Windows. Przetestowane z Firefox, Chromium, VS Code, Kate, Dolphin,
LibreOffice, GIMP, aplikacjami GTK4, foot, kitty, xterm — na VM (llvmpipe, jeden monitor wirtualny).

## Do zrobienia

1. **Weryfikacja na prawdziwym sprzęcie** (to jest powód przenosin): 3 monitory, prawdziwe GPU,
   mieszane DPI / skalowanie (belki, quick-tile, `move_to_monitor`, pamięć geometrii przy różnych
   skalach), tryb nested testów, wydajność animacji.
2. Aplikacje jeszcze nietestowane: Steam, JetBrains (XWayland, okna bez dekoracji, splash), gry
   pełnoekranowe, Discord/Electron z tray, okna wielookienkowe (GIMP single/multi window).
3. Prawdziwy shell w Quickshellu (panel/taskbar, Alt+Tab, menu okna, tray, powiadomienia) — osobny
   projekt; dev-shell (`scripts/e2e/dev-shell/`) pokazuje, jak używać IPC (`set_rectangle` dla
   animacji minimalizacji, `focusHistoryID` dla MRU, event `windowmenu`). Znany problem dev-shella:
   pierwsze Alt+Tab po starcie bywa zawodne.
4. Paczka Arch (PKGBUILD, `provides/conflicts=hyprland`) zamiast instalacji do `/usr/local`.
5. Regularny merge upstreamu + CI (build + grupa `stack`) na GitHubie.
6. Zbadać `dwindleFloatClamp` (niski priorytet).
