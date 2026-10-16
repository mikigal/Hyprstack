-- Base config appended to generated hs-session configs (monitors are prepended).

hl.config({
    debug = { enable_stdout_logs = true, disable_logs = false },
    misc = {
        disable_hyprland_logo = true,
        disable_splash_rendering = true,
    },
    animations = { enabled = false },
})

hl.config({
    general = { border_size = 1, gaps_in = 0, gaps_out = 0 },
    decoration = { rounding = 8, shadow = { enabled = true, range = 12, render_power = 3, color = "rgba(00000066)" } },
})
