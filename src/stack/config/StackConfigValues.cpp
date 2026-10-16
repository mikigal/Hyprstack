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
    };

#undef MS
}
