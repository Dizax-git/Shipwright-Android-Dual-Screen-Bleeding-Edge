#pragma once

#include <algorithm>
#include <cstdint>

namespace LUS {

enum class ModifierStickDirection : uint8_t {
    None,
    Left,
    Right,
    Up,
    Down,
};

struct ModifierStickCButtonsState {
    ModifierStickDirection direction = ModifierStickDirection::None;
};

struct ModifierStickCButtonsConfig {
    bool enabled = false;
    uint16_t modifierMask = 0x0040;
    uint16_t upMask = 0x0008;
    uint16_t downMask = 0x0004;
    uint16_t leftMask = 0x0002;
    uint16_t rightMask = 0x0001;
    int16_t activationThreshold = 45;
    int16_t releaseThreshold = 30;
};

namespace detail {

inline int16_t DirectionStrength(ModifierStickDirection direction, int16_t x, int16_t y) {
    switch (direction) {
        case ModifierStickDirection::Left:
            return std::max<int16_t>(-x, 0);
        case ModifierStickDirection::Right:
            return std::max<int16_t>(x, 0);
        case ModifierStickDirection::Up:
            return std::max<int16_t>(y, 0);
        case ModifierStickDirection::Down:
            return std::max<int16_t>(-y, 0);
        case ModifierStickDirection::None:
        default:
            return 0;
    }
}

inline ModifierStickDirection DominantDirection(int16_t x, int16_t y, int16_t threshold) {
    const int16_t absX = static_cast<int16_t>(x < 0 ? -x : x);
    const int16_t absY = static_cast<int16_t>(y < 0 ? -y : y);
    if (std::max(absX, absY) < threshold) {
        return ModifierStickDirection::None;
    }
    if (absX >= absY) {
        return x < 0 ? ModifierStickDirection::Left : ModifierStickDirection::Right;
    }
    return y < 0 ? ModifierStickDirection::Down : ModifierStickDirection::Up;
}

inline uint16_t DirectionMask(ModifierStickDirection direction, const ModifierStickCButtonsConfig& config) {
    switch (direction) {
        case ModifierStickDirection::Left:
            return config.leftMask;
        case ModifierStickDirection::Right:
            return config.rightMask;
        case ModifierStickDirection::Up:
            return config.upMask;
        case ModifierStickDirection::Down:
            return config.downMask;
        case ModifierStickDirection::None:
        default:
            return 0;
    }
}

} // namespace detail

template <typename Pad>
void ApplyModifierStickCButtons(Pad& pad, ModifierStickCButtonsState& state,
                                const ModifierStickCButtonsConfig& config) {
    if (!config.enabled || (pad.button & config.modifierMask) == 0) {
        state.direction = ModifierStickDirection::None;
        return;
    }

    const int16_t x = pad.stick_x;
    const int16_t y = pad.stick_y;
    pad.stick_x = 0;
    pad.stick_y = 0;

    const auto preferred = detail::DominantDirection(x, y, config.activationThreshold);
    if (state.direction == ModifierStickDirection::None) {
        state.direction = preferred;
    } else {
        const int16_t currentStrength = detail::DirectionStrength(state.direction, x, y);
        const int16_t preferredStrength = detail::DirectionStrength(preferred, x, y);
        if (preferred != ModifierStickDirection::None && preferred != state.direction &&
            preferredStrength > currentStrength) {
            state.direction = preferred;
        } else if (currentStrength < config.releaseThreshold) {
            state.direction = preferred;
        }
    }

    pad.button |= detail::DirectionMask(state.direction, config);
}

} // namespace LUS
