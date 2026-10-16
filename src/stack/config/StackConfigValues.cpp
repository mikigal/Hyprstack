#include "StackConfigValues.hpp"

using namespace Config::Values;

std::vector<SP<IValue>> Stack::Config::getConfigValues() {
#define MS makeConfigValue

    return std::vector<SP<IValue>>{

        /*
         * stacking:
         */

        MS<String>("stacking:placement", "where new windows are placed. [smart/center/cascade/mouse]", "smart", {.validator = strChoice({"smart", "center", "cascade", "mouse"})}),
        MS<Bool>("stacking:center_transients", "center dialogs and other transient windows over their parent", true),
        MS<Int>("stacking:cascade_offset", "offset in logical pixels between cascaded windows", 32, {.min = 0, .max = 500}),
        MS<Bool>("stacking:raise_on_focus", "raise windows when they get focused by keyboard, dispatchers or activation (clicks always raise)", true),
        MS<Bool>("stacking:clamp_new_to_workarea", "shrink new windows that do not fit into the work area", true),
        MS<String>("stacking:remember_geometry", "remember window geometry per app when it closes. [none/size/all] (all = size and position)", "all",
                   {.validator = strChoice({"none", "size", "all"})}),
        MS<String>("stacking:remember_geometry_file", "where remembered geometry is stored, empty for $XDG_STATE_HOME/hyprstack/window-geometry", ""),
        MS<Bool>("stacking:minimize_animation", "minimizing shrinks the window into its taskbar button (needs animations:enabled)", true),
        MS<Bool>("stacking:window_menu", "tell clients a window menu exists (the shell draws it, see the windowmenu event)", true),
        MS<Bool>("stacking:decorate_csd", "draw border, shadow and rounding around windows that decorate themselves (GTK & co.)", false),

        /*
         * stacking:quick_tile:
         */

        MS<Bool>("stacking:quick_tile:enabled", "tile windows by dragging them against outer screen edges (half) and corners (quarter)", true),
        MS<Int>("stacking:quick_tile:edge_threshold", "how close to a screen edge the cursor has to be, in logical pixels", 4, {.min = 0, .max = 100}),
        MS<Int>("stacking:quick_tile:corner_size", "size of the corner areas that tile to a quarter, in logical pixels", 80, {.min = 0, .max = 1000}),
        MS<String>("stacking:quick_tile:top_action", "what dragging against the top edge does. [maximize/tile/none]", "maximize",
                   {.validator = strChoice({"maximize", "tile", "none"})}),
        MS<Color>("stacking:quick_tile:col.preview", "fill color of the quick-tile preview", 0x33ffffff),
        MS<Color>("stacking:quick_tile:col.preview_border", "border color of the quick-tile preview", 0x99ffffff),

        /*
         * stacking:titlebar:
         */

        MS<Bool>("stacking:titlebar:enabled", "draw server-side titlebars for windows that do not decorate themselves", true),
        MS<Int>("stacking:titlebar:height", "titlebar height in logical pixels", 32, {.min = 0, .max = 200}),
        MS<String>("stacking:titlebar:font_family", "titlebar font, empty to use misc:font_family", ""),
        MS<Int>("stacking:titlebar:font_size", "titlebar font size in points", 10, {.min = 4, .max = 72}),
        MS<String>("stacking:titlebar:text_align", "title alignment. [left/center]", "left", {.validator = strChoice({"left", "center"})}),
        MS<Int>("stacking:titlebar:padding", "horizontal padding of the title in logical pixels", 12, {.min = 0, .max = 200}),
        MS<String>("stacking:titlebar:buttons", "comma-separated buttons on the right side, left to right. [minimize/maximize/close]", "minimize,maximize,close"),
        MS<Int>("stacking:titlebar:button_width", "width of a titlebar button in logical pixels", 46, {.min = 8, .max = 200}),
        MS<Color>("stacking:titlebar:col.active", "titlebar color of the focused window", 0xff202020),
        MS<Color>("stacking:titlebar:col.inactive", "titlebar color of unfocused windows", 0xff2c2c2c),
        MS<Color>("stacking:titlebar:col.text_active", "title color of the focused window", 0xffffffff),
        MS<Color>("stacking:titlebar:col.text_inactive", "title color of unfocused windows", 0xff8c8c8c),
        MS<Color>("stacking:titlebar:col.button_hover", "background of a hovered button", 0x1affffff),
        MS<Color>("stacking:titlebar:col.button_pressed", "background of a pressed button", 0x33ffffff),
        MS<Color>("stacking:titlebar:col.close_hover", "background of the hovered close button", 0xffc42b1c),
        MS<String>("stacking:titlebar:double_click", "action on titlebar double click. [maximize/minimize/none]", "maximize",
                   {.validator = strChoice({"maximize", "minimize", "none"})}),
    };

#undef MS
}
