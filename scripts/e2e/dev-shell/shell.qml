// Hyprstack development shell — a minimal taskbar + Alt+Tab switcher used to exercise the
// compositor's shell-facing features in E2E tests. NOT the real shell.
//
//   quickshell -p scripts/e2e/dev-shell
//
// Compositor config needed for the switcher:
//   hl.bind("ALT + Tab", hl.dsp.global("hyprstack:alttab"))
//   hl.bind("ALT + SHIFT + Tab", hl.dsp.global("hyprstack:alttabback"))
import QtQuick
import Quickshell

ShellRoot {
    Variants {
        model: Quickshell.screens
        delegate: Taskbar {}
    }

    Switcher {}

    WindowMenu {}
}
