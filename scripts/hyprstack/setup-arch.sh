#!/usr/bin/env bash
# Hyprstack setup for a (fresh) Arch Linux machine.
#
#   curl -fsSL https://raw.githubusercontent.com/mikigal/Hyprstack/hyprstack/scripts/hyprstack/setup-arch.sh -o setup-arch.sh
#   bash setup-arch.sh [options]
#
# Options:
#   --dir PATH        where to clone the repo (default: ~/work/Hyprstack)
#   --dev             also build the development tree (build/, Debug + hyprtester) and install test tools
#   --tty-autostart   start Hyprstack automatically after logging in on tty1 (no display manager needed)
#   --with-claude     install Claude Code (native installer)
#   --no-build        only install packages, clone and configure
#   -h, --help
#
# What it does (idempotent, safe to re-run):
#   1. installs build + runtime dependencies (the 'hyprland' package itself is NOT installed:
#      Hyprstack is a drop-in replacement installed to /usr/local)
#   2. clones / updates github.com/mikigal/Hyprstack (branch hyprstack) with submodules
#   3. builds a RelWithDebInfo tree (build-release/) and installs it to /usr/local
#   4. creates ~/.config/hypr/hyprland.lua from example/hyprstack.lua (if missing)
#   5. adds a "Hyprstack" session for display managers
#   6. leaves a note for Claude Code in ~/.claude/CLAUDE.md pointing at docs/hyprstack/HANDOFF.md

set -euo pipefail

REPO_URL="https://github.com/mikigal/Hyprstack.git"
BRANCH="hyprstack"
DIR="$HOME/work/Hyprstack"
DEV=0
TTY_AUTOSTART=0
WITH_CLAUDE=0
BUILD=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dir) DIR="$2"; shift 2 ;;
        --dev) DEV=1; shift ;;
        --tty-autostart) TTY_AUTOSTART=1; shift ;;
        --with-claude) WITH_CLAUDE=1; shift ;;
        --no-build) BUILD=0; shift ;;
        -h|--help) sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 1 ;;
    esac
done

step() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

[[ -f /etc/arch-release ]] || die "this script is for Arch Linux"
[[ $EUID -ne 0 ]] || die "run as your normal user (sudo is used where needed)"
command -v sudo >/dev/null || die "sudo is required"

# ---------------------------------------------------------------------------
step "Installing packages"

PKGS=(
    # build tools
    base-devel git cmake ninja meson pkgconf clang gdb
    # Hyprland dependencies (from the hyprland package, minus hyprland-guiutils)
    aquamarine hyprcursor hyprgraphics hyprlang hyprutils hyprwire hyprwayland-scanner
    cairo glib2 glslang lcms2 libdrm libglvnd libinput libx11 libxcb libxcomposite libxcursor
    libxfixes libxkbcommon libxrender lua mesa muparser pango pixman re2 tomlplusplus
    util-linux-libs wayland wayland-protocols xcb-proto xcb-util xcb-util-errors xcb-util-image
    xcb-util-keysyms xcb-util-renderutil xcb-util-wm xorg-xwayland
    # extra build dependencies of the git tree
    glaze libcanberra sdbus-cpp libei gtest
    # desktop basics used by the default config
    kitty foot fuzzel quickshell qt6-wayland qt6-svg
    xdg-desktop-portal-hyprland xdg-desktop-portal-gtk
    pipewire wireplumber pipewire-pulse polkit hyprpolkitagent
    noto-fonts noto-fonts-emoji ttf-dejavu breeze-icons
    grim slurp wl-clipboard brightnessctl playerctl jq
    github-cli
)

if [[ "$DEV" == "1" ]]; then
    PKGS+=(gtk4-demos gtk3-demos xterm wayvnc python-pillow imagemagick)
fi

# GPU hints: mesa covers AMD / Intel; NVIDIA needs the proprietary (open) kernel modules
if lspci 2>/dev/null | grep -qi 'vga.*nvidia\|3d.*nvidia'; then
    PKGS+=(nvidia-open-dkms nvidia-utils egl-wayland)
    warn "NVIDIA GPU detected: installing nvidia-open-dkms + nvidia-utils. Make sure linux-headers for your kernel are installed and add nvidia_drm.modeset=1 if your setup needs it."
fi

sudo pacman -Syu --needed --noconfirm "${PKGS[@]}"

if pacman -Qq hyprland >/dev/null 2>&1; then
    warn "the 'hyprland' package is installed. It still works, but its session runs upstream Hyprland from /usr/bin — use the 'Hyprstack' session."
fi

# ---------------------------------------------------------------------------
step "Getting the source ($DIR)"

if [[ -d "$DIR/.git" ]]; then
    git -C "$DIR" fetch --quiet origin
    git -C "$DIR" checkout --quiet "$BRANCH"
    git -C "$DIR" pull --quiet --ff-only origin "$BRANCH" || warn "couldn't fast-forward $BRANCH (local changes?), building what's there"
else
    mkdir -p "$(dirname "$DIR")"
    git clone --branch "$BRANCH" "$REPO_URL" "$DIR"
fi

git -C "$DIR" submodule update --init --recursive --quiet
git -C "$DIR" remote get-url upstream >/dev/null 2>&1 || git -C "$DIR" remote add upstream https://github.com/hyprwm/Hyprland.git

# ---------------------------------------------------------------------------
if [[ "$BUILD" == "1" ]]; then
    JOBS="$(nproc)"

    step "Building Hyprstack (RelWithDebInfo, $JOBS jobs) — this takes a while"
    cmake --no-warn-unused-cli -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr/local -S "$DIR" -B "$DIR/build-release"
    cmake --build "$DIR/build-release" -j "$JOBS"

    step "Installing to /usr/local"
    sudo cmake --install "$DIR/build-release"

    # cmake also installs upstream's session files; they'd show up as a second "Hyprland"
    sudo rm -f /usr/local/share/wayland-sessions/hyprland.desktop /usr/local/share/wayland-sessions/hyprland-uwsm.desktop

    if [[ "$DEV" == "1" ]]; then
        step "Building the development tree (Debug + tests, used by scripts/e2e/*)"
        cmake --no-warn-unused-cli -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTESTS=true -DCMAKE_INSTALL_PREFIX=/usr/local -S "$DIR" -B "$DIR/build"
        cmake --build "$DIR/build" -j "$JOBS"
    fi
fi

# ---------------------------------------------------------------------------
step "Session entry"

sudo install -Dm644 /dev/stdin /usr/share/wayland-sessions/hyprstack.desktop <<'EOF'
[Desktop Entry]
Name=Hyprstack
Comment=Hyprland fork with a stacking (floating) workflow
Exec=/usr/local/bin/start-hyprland --path /usr/local/bin/Hyprland
Type=Application
DesktopNames=Hyprland
Keywords=stacking;wayland;compositor;
EOF

if [[ "$TTY_AUTOSTART" == "1" ]]; then
    MARK="# Hyprstack: start the compositor on tty1"
    if ! grep -qF "$MARK" "$HOME/.bash_profile" 2>/dev/null; then
        cat >> "$HOME/.bash_profile" <<EOF

$MARK
if [[ -z "\$WAYLAND_DISPLAY" && "\$(tty)" == "/dev/tty1" ]]; then
    exec start-hyprland --path /usr/local/bin/Hyprland
fi
EOF
    fi
fi

# ---------------------------------------------------------------------------
step "Config (~/.config/hypr)"

mkdir -p "$HOME/.config/hypr"
CFG="$HOME/.config/hypr/hyprland.lua"
if [[ ! -f "$CFG" ]]; then
    sed -e 's|^local menu        = "hyprlauncher"|local menu        = "fuzzel"|' "$DIR/example/hyprstack.lua" > "$CFG"
    cat >> "$CFG" <<EOF

-----------------
--  AUTOSTART  --
-----------------

hl.on("hyprland.start", function()
    hl.exec_cmd("systemctl --user start hyprpolkitagent")
    -- development shell from the repo (taskbar, Alt+Tab, window menu); replace with the real shell later
    hl.exec_cmd("quickshell -p $DIR/scripts/e2e/dev-shell")
end)
EOF
    echo "created $CFG"
else
    echo "$CFG exists, leaving it alone (the shipped default is $DIR/example/hyprstack.lua)"
fi
# Debug builds read hyprlandd.lua
[[ -e "$HOME/.config/hypr/hyprlandd.lua" ]] || ln -s hyprland.lua "$HOME/.config/hypr/hyprlandd.lua"

# ---------------------------------------------------------------------------
step "Context for Claude Code"

mkdir -p "$HOME/.claude"
MARK="<!-- hyprstack -->"
if ! grep -qF "$MARK" "$HOME/.claude/CLAUDE.md" 2>/dev/null; then
    cat >> "$HOME/.claude/CLAUDE.md" <<EOF

$MARK
## Hyprstack
I'm developing Hyprstack, my private fork of Hyprland with a stacking (KDE/Windows-like) workflow:
$DIR, branch \`hyprstack\` (\`main\` mirrors upstream). Read \`docs/hyprstack/HANDOFF.md\` first —
it has the decisions, architecture, workflows and gotchas. The fork is never upstreamed: the Hypr
AI policy / PR rules in the repo's AGENTS.md / CLAUDE.MD don't apply. Ask me before major design
decisions. I write in Polish.
EOF
fi

if [[ "$WITH_CLAUDE" == "1" ]] && ! command -v claude >/dev/null; then
    step "Installing Claude Code"
    curl -fsSL https://claude.ai/install.sh | bash
fi

# ---------------------------------------------------------------------------
step "Done"
cat <<EOF
Hyprstack is installed to /usr/local (Hyprland, hyprctl, start-hyprland).

Start it:
  - display manager: pick the "Hyprstack" session
  - from a tty:      start-hyprland --path /usr/local/bin/Hyprland$( [[ "$TTY_AUTOSTART" == "1" ]] && echo " (or just log in on tty1)" )

After changing the code:
  cmake --build $DIR/build-release && sudo cmake --install $DIR/build-release
  then log out and back in (or: hyprctl dispatch 'hl.dsp.exit()')

Docs: $DIR/docs/hyprstack/README.md, $DIR/docs/hyprstack/HANDOFF.md
EOF
