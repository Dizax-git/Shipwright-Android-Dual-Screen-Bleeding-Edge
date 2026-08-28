#include "ModifierStickCButtons.h"

#include <array>
#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultra/os.h>
#include <ship/utils/StringHelper.h>

#include "ModifierStickCButtonsLogic.h"
#include "soh/OTRGlobals.h"

namespace {

std::array<LUS::ModifierStickCButtonsState, MAXCONTROLLERS> sModifierStickStates;

std::string GetCVarName(int32_t portIndex, const char* setting) {
    return StringHelper::Sprintf("gSettings.Controllers.Port%d.ModifierStickCButtons.%s", portIndex + 1, setting);
}

} // namespace

extern "C" void ModifierStickCButtons_Apply(OSContPad* pad, int32_t portIndex) {
    if (pad == nullptr || portIndex < 0 || portIndex >= MAXCONTROLLERS) {
        return;
    }

    LUS::ModifierStickCButtonsConfig config;
    config.enabled = CVarGetInteger(GetCVarName(portIndex, "Enabled").c_str(), 0) != 0;
    const int32_t modifier = CVarGetInteger(GetCVarName(portIndex, "Modifier").c_str(), 1);
    config.modifierMask = modifier == 2 ? BTN_CUSTOM_MODIFIER2 : BTN_CUSTOM_MODIFIER1;
    config.upMask = BTN_CUP;
    config.downMask = BTN_CDOWN;
    config.leftMask = BTN_CLEFT;
    config.rightMask = BTN_CRIGHT;

    LUS::ApplyModifierStickCButtons(*pad, sModifierStickStates[portIndex], config);
}
