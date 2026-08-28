#include <cassert>
#include <cstdint>
#include <iostream>

#include "soh/soh/Enhancements/controls/ModifierStickCButtonsLogic.h"

namespace {

constexpr uint16_t BTN_CRIGHT_TEST = 0x0001;
constexpr uint16_t BTN_CLEFT_TEST = 0x0002;
constexpr uint16_t BTN_CDOWN_TEST = 0x0004;
constexpr uint16_t BTN_CUP_TEST = 0x0008;
constexpr uint16_t BTN_MODIFIER_TEST = 0x0040;
constexpr uint16_t BTN_UNRELATED_TEST = 0x8000;

struct TestPad {
    uint16_t button = 0;
    int8_t stick_x = 0;
    int8_t stick_y = 0;
    int8_t right_stick_x = 0;
    int8_t right_stick_y = 0;
    float gyro_x = 0.0F;
    float gyro_y = 0.0F;
};

LUS::ModifierStickCButtonsConfig EnabledConfig() {
    return {
        .enabled = true,
        .modifierMask = BTN_MODIFIER_TEST,
        .upMask = BTN_CUP_TEST,
        .downMask = BTN_CDOWN_TEST,
        .leftMask = BTN_CLEFT_TEST,
        .rightMask = BTN_CRIGHT_TEST,
        .activationThreshold = 45,
        .releaseThreshold = 30,
    };
}

void TestDisabledPassThrough() {
    auto config = EnabledConfig();
    config.enabled = false;
    LUS::ModifierStickCButtonsState state{};
    TestPad pad{BTN_MODIFIER_TEST | BTN_UNRELATED_TEST, 70, -50, 12, -13, 1.25F, -2.5F};
    const TestPad original = pad;

    LUS::ApplyModifierStickCButtons(pad, state, config);

    assert(pad.button == original.button);
    assert(pad.stick_x == original.stick_x && pad.stick_y == original.stick_y);
    assert(pad.right_stick_x == original.right_stick_x && pad.right_stick_y == original.right_stick_y);
    assert(pad.gyro_x == original.gyro_x && pad.gyro_y == original.gyro_y);
    assert(state.direction == LUS::ModifierStickDirection::None);
}

void TestModifierFreePassThroughAndReset() {
    auto config = EnabledConfig();
    LUS::ModifierStickCButtonsState state{LUS::ModifierStickDirection::Right};
    TestPad pad{BTN_UNRELATED_TEST, 70, 0};

    LUS::ApplyModifierStickCButtons(pad, state, config);

    assert(pad.button == BTN_UNRELATED_TEST);
    assert(pad.stick_x == 70 && pad.stick_y == 0);
    assert(state.direction == LUS::ModifierStickDirection::None);
}

void TestCardinalDirections() {
    struct Case {
        int8_t x;
        int8_t y;
        uint16_t expected;
        LUS::ModifierStickDirection direction;
    };
    const Case cases[] = {
        {70, 0, BTN_CRIGHT_TEST, LUS::ModifierStickDirection::Right},
        {-70, 0, BTN_CLEFT_TEST, LUS::ModifierStickDirection::Left},
        {0, 70, BTN_CUP_TEST, LUS::ModifierStickDirection::Up},
        {0, -70, BTN_CDOWN_TEST, LUS::ModifierStickDirection::Down},
    };

    for (const auto& test : cases) {
        LUS::ModifierStickCButtonsState state{};
        TestPad pad{BTN_MODIFIER_TEST | BTN_UNRELATED_TEST, test.x, test.y, 11, -12, 2.0F, -3.0F};
        LUS::ApplyModifierStickCButtons(pad, state, EnabledConfig());
        assert((pad.button & test.expected) != 0);
        assert((pad.button & BTN_UNRELATED_TEST) != 0);
        assert(pad.stick_x == 0 && pad.stick_y == 0);
        assert(pad.right_stick_x == 11 && pad.right_stick_y == -12);
        assert(pad.gyro_x == 2.0F && pad.gyro_y == -3.0F);
        assert(state.direction == test.direction);
    }
}

void TestNeutralSuppressesMovement() {
    LUS::ModifierStickCButtonsState state{};
    TestPad pad{BTN_MODIFIER_TEST, 20, -20};
    LUS::ApplyModifierStickCButtons(pad, state, EnabledConfig());
    assert(pad.stick_x == 0 && pad.stick_y == 0);
    assert((pad.button & (BTN_CUP_TEST | BTN_CDOWN_TEST | BTN_CLEFT_TEST | BTN_CRIGHT_TEST)) == 0);
    assert(state.direction == LUS::ModifierStickDirection::None);
}

void TestDominantAxisAndEqualPriority() {
    auto config = EnabledConfig();
    LUS::ModifierStickCButtonsState state{};
    TestPad vertical{BTN_MODIFIER_TEST, 55, 70};
    LUS::ApplyModifierStickCButtons(vertical, state, config);
    assert((vertical.button & BTN_CUP_TEST) != 0);

    state = {};
    TestPad equal{BTN_MODIFIER_TEST, -70, 70};
    LUS::ApplyModifierStickCButtons(equal, state, config);
    assert((equal.button & BTN_CLEFT_TEST) != 0);
    assert(state.direction == LUS::ModifierStickDirection::Left);
}

void TestHysteresisSwitchAndRelease() {
    auto config = EnabledConfig();
    LUS::ModifierStickCButtonsState state{};

    TestPad activate{BTN_MODIFIER_TEST, 60, 0};
    LUS::ApplyModifierStickCButtons(activate, state, config);
    assert(state.direction == LUS::ModifierStickDirection::Right);

    TestPad heldBelowActivation{BTN_MODIFIER_TEST, 35, 0};
    LUS::ApplyModifierStickCButtons(heldBelowActivation, state, config);
    assert((heldBelowActivation.button & BTN_CRIGHT_TEST) != 0);
    assert(state.direction == LUS::ModifierStickDirection::Right);

    TestPad switchDirection{BTN_MODIFIER_TEST, 35, 65};
    LUS::ApplyModifierStickCButtons(switchDirection, state, config);
    assert((switchDirection.button & BTN_CUP_TEST) != 0);
    assert(state.direction == LUS::ModifierStickDirection::Up);

    TestPad release{BTN_MODIFIER_TEST, 0, 20};
    LUS::ApplyModifierStickCButtons(release, state, config);
    assert((release.button & (BTN_CUP_TEST | BTN_CDOWN_TEST | BTN_CLEFT_TEST | BTN_CRIGHT_TEST)) == 0);
    assert(state.direction == LUS::ModifierStickDirection::None);
}

} // namespace

int main() {
    TestDisabledPassThrough();
    TestModifierFreePassThroughAndReset();
    TestCardinalDirections();
    TestNeutralSuppressesMovement();
    TestDominantAxisAndEqualPriority();
    TestHysteresisSwitchAndRelease();
    std::cout << "modifier-stick-c-buttons: PASS\n";
    return 0;
}
