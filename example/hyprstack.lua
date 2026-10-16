-- Hyprstack default config: a stacking (floating) desktop with Windows-like controls.
-- Hyprstack is a Hyprland fork; everything from the Hyprland wiki applies, plus the
-- stacking options documented in docs/hyprstack/DESIGN.md.

----------------
--  MONITORS  --
----------------

hl.monitor({
    output   = "",
    mode     = "preferred",
    position = "auto",
    scale    = "auto",
})

-------------------
--  MY PROGRAMS  --
-------------------

local terminal    = "kitty"
local fileManager = "dolphin"
local menu        = "hyprlauncher"

-------------
--  ENV    --
-------------

hl.env("XCURSOR_SIZE", "24")
hl.env("HYPRCURSOR_SIZE", "24")

---------------------
--  LOOK AND FEEL  --
---------------------

hl.config({
    general = {
        layout      = "stacking",
        gaps_in     = 0,
        gaps_out    = 0,
        border_size = 1,
        col = {
            active_border   = "rgba(4c8ed9ff)",
            inactive_border = "rgba(3a3a3aff)",
        },

        -- resize windows by dragging their edges
        resize_on_border        = true,
        extend_border_grab_area = 8,
        hover_icon_on_border    = true,

        -- magnetic edges while moving windows
        snap = {
            enabled      = true,
            window_gap   = 10,
            monitor_gap  = 10,
            respect_gaps = false,
        },
    },

    decoration = {
        rounding       = 8,
        rounding_power = 2,
        shadow = {
            enabled      = true,
            range        = 18,
            render_power = 3,
            color        = 0x66000000,
            color_inactive = 0x33000000,
        },
        blur = {
            enabled = true,
            size    = 4,
            passes  = 2,
        },
    },

    stacking = {
        placement = "smart", -- smart / center / cascade / mouse

        titlebar = {
            enabled      = true,
            height       = 32,
            text_align   = "left",
            buttons      = "minimize,maximize,close",
            double_click = "maximize",
        },

        quick_tile = {
            enabled    = true,
            top_action = "maximize",
        },
    },

    animations = {
        enabled = true,
    },
})

hl.curve("easeOutQuint", { type = "bezier", points = { {0.23, 1}, {0.32, 1} } })
hl.curve("linear",       { type = "bezier", points = { {0, 0},    {1, 1}    } })
hl.curve("almostLinear", { type = "bezier", points = { {0.5, 0.5}, {0.75, 1} } })
hl.curve("quick",        { type = "bezier", points = { {0.15, 0}, {0.1, 1}  } })

hl.animation({ leaf = "global",     enabled = true, speed = 10,   bezier = "default" })
hl.animation({ leaf = "border",     enabled = true, speed = 5.39, bezier = "easeOutQuint" })
hl.animation({ leaf = "windows",    enabled = true, speed = 3.5,  bezier = "easeOutQuint" })
hl.animation({ leaf = "windowsIn",  enabled = true, speed = 3,    bezier = "easeOutQuint", style = "popin 90%" })
hl.animation({ leaf = "windowsOut", enabled = true, speed = 1.5,  bezier = "linear",       style = "popin 90%" })
hl.animation({ leaf = "fadeIn",     enabled = true, speed = 1.73, bezier = "almostLinear" })
hl.animation({ leaf = "fadeOut",    enabled = true, speed = 1.46, bezier = "almostLinear" })
hl.animation({ leaf = "fade",       enabled = true, speed = 3.03, bezier = "quick" })
hl.animation({ leaf = "layers",     enabled = true, speed = 3.81, bezier = "easeOutQuint" })
hl.animation({ leaf = "layersIn",   enabled = true, speed = 4,    bezier = "easeOutQuint", style = "fade" })
hl.animation({ leaf = "layersOut",  enabled = true, speed = 1.5,  bezier = "linear",       style = "fade" })

hl.config({
    misc = {
        force_default_wallpaper = 0,
        disable_hyprland_logo   = true,
        background_color        = 0xff1e2a3a,
        -- apps started from a launcher / clicked links get focus
        focus_on_activate = true,
    },

    cursor = {
        -- the cursor stays where you put it
        no_warps = true,
    },

    -- upstream Hyprland release notes / donation popups don't apply to this fork
    ecosystem = {
        no_update_news   = true,
        no_donation_nag  = true,
    },
})

-------------
--  INPUT  --
-------------

hl.config({
    input = {
        kb_layout = "us",

        -- click to focus; the pointer still talks to whatever is under it (scrolling etc.)
        follow_mouse = 2,

        touchpad = {
            natural_scroll = false,
        },
    },
})

-------------------
--  KEYBINDINGS  --
-------------------

local mod = "SUPER"

-- Windows-like window management
hl.bind("ALT + F4",            hl.dsp.window.close())
hl.bind(mod .. " + left",      hl.dsp.window.snap({ direction = "left" }))
hl.bind(mod .. " + right",     hl.dsp.window.snap({ direction = "right" }))
hl.bind(mod .. " + up",        hl.dsp.window.snap({ direction = "up" }))
hl.bind(mod .. " + down",      hl.dsp.window.snap({ direction = "down" }))
hl.bind(mod .. " + SHIFT + left",  hl.dsp.window.move_to_monitor({ direction = "left" }))
hl.bind(mod .. " + SHIFT + right", hl.dsp.window.move_to_monitor({ direction = "right" }))
hl.bind(mod .. " + D",         hl.dsp.show_desktop())
hl.bind(mod .. " + F11",       hl.dsp.window.fullscreen())
hl.bind("ALT + space",         hl.dsp.window.menu()) -- window menu, drawn by the shell

-- window switcher: drawn by the shell (Quickshell), the compositor only forwards the shortcut
hl.bind("ALT + Tab",           hl.dsp.global("hyprstack:alttab"))
hl.bind("ALT + SHIFT + Tab",   hl.dsp.global("hyprstack:alttabback"))

-- programs
hl.bind(mod .. " + Return",    hl.dsp.exec_cmd(terminal))
hl.bind(mod .. " + E",         hl.dsp.exec_cmd(fileManager))
hl.bind(mod .. " + R",         hl.dsp.exec_cmd(menu))
hl.bind(mod .. " + SHIFT + E", hl.dsp.exec_cmd("command -v hyprshutdown >/dev/null 2>&1 && hyprshutdown || hyprctl dispatch 'hl.dsp.exit()'"))

-- move / resize with Super + mouse (titlebars and window edges work too)
hl.bind(mod .. " + mouse:272", hl.dsp.window.drag(),   { mouse = true })
hl.bind(mod .. " + mouse:273", hl.dsp.window.resize(), { mouse = true })
-- like KDE / X11 desktops:
-- hl.bind("ALT + mouse:272", hl.dsp.window.drag(),   { mouse = true })
-- hl.bind("ALT + mouse:273", hl.dsp.window.resize(), { mouse = true })

-- media keys
hl.bind("XF86AudioRaiseVolume",  hl.dsp.exec_cmd("wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ 5%+"), { locked = true, repeating = true })
hl.bind("XF86AudioLowerVolume",  hl.dsp.exec_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"),      { locked = true, repeating = true })
hl.bind("XF86AudioMute",         hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle"),     { locked = true, repeating = true })
hl.bind("XF86AudioMicMute",      hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle"),   { locked = true, repeating = true })
hl.bind("XF86MonBrightnessUp",   hl.dsp.exec_cmd("brightnessctl -e4 -n2 set 5%+"),                  { locked = true, repeating = true })
hl.bind("XF86MonBrightnessDown", hl.dsp.exec_cmd("brightnessctl -e4 -n2 set 5%-"),                  { locked = true, repeating = true })
hl.bind("XF86AudioNext",  hl.dsp.exec_cmd("playerctl next"),       { locked = true })
hl.bind("XF86AudioPause", hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPlay",  hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPrev",  hl.dsp.exec_cmd("playerctl previous"),   { locked = true })

--------------------
--  WINDOW RULES  --
--------------------

-- XWayland drag & drop helper windows
hl.window_rule({
    name  = "fix-xwayland-drags",
    match = {
        class      = "^$",
        title      = "^$",
        xwayland   = true,
        float      = true,
        fullscreen = false,
        pin        = false,
    },
    no_focus = true,
})
