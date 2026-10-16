# Shared helpers for scripts/e2e/* (sourced).
#
# Two ways to run a test compositor:
#   - nested:   inside a running Wayland session (WAYLAND_DISPLAY set) — e.g. on a real machine
#               while working in Hyprstack itself. The test instance renders into its own window
#               and never touches the GPU directly. Needs a real GPU (render node): with software
#               rendering (llvmpipe, e.g. a VM) GBM wants dumb buffers on the card node owned by
#               the desktop session and every output is rejected (0x0) — use headless there.
#   - headless: no session (e.g. over ssh on a VM) — it opens the DRM device itself (noop seat),
#               which only works while no desktop session owns the GPU.

# Hyprland instances that are not a desktop session (those are started by start-hyprland)
hs_test_compositors() {
    local pid ppid
    for pid in $(pgrep -x Hyprland); do
        ppid="$(ps -o ppid= -p "$pid" | tr -d ' ')"
        [[ "$(ps -o comm= -p "$ppid" 2>/dev/null)" == "start-hyprland" ]] && continue
        echo "$pid"
    done
}

hs_kill_test_compositors() {
    local pids
    pids="$(hs_test_compositors)"
    [[ -z "$pids" ]] && return 0
    # shellcheck disable=SC2086
    kill $pids 2>/dev/null || true
    for _ in $(seq 1 30); do
        [[ -z "$(hs_test_compositors)" ]] && return 0
        sleep 0.1
    done
    # shellcheck disable=SC2046
    kill -9 $(hs_test_compositors) 2>/dev/null || true
}

# exports the environment for a test compositor, prints the mode
hs_setup_backend() {
    if [[ -n "${WAYLAND_DISPLAY:-}" ]]; then
        # nested: keep aquamarine off the DRM device, it gets buffers from the parent session
        export AQ_DRM_DEVICES=/nonexistent-hyprstack-nested
        unset LIBSEAT_BACKEND
        echo nested
        return 0
    fi

    if pgrep -x start-hyprland >/dev/null; then
        echo "a desktop session (start-hyprland) owns the GPU." >&2
        echo "  run from inside it (or export WAYLAND_DISPLAY=<its socket>) to test nested," >&2
        echo "  or stop it (VM: sudo systemctl stop getty@tty1)" >&2
        return 1
    fi

    export LIBSEAT_BACKEND=noop
    unset AQ_DRM_DEVICES
    echo headless
}
