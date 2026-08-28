# Modifier + Left Stick C-Button Layer

## Goal

Add an optional controller layer that keeps the physical left stick as the N64
analog stick normally, but converts it to the four N64 C-buttons while a mapped
modifier is held. This permits a conventional modern layout with movement on
the left stick, camera and aiming on the right stick, A/B on the corresponding
face buttons, Z on L2, R on R1, and C-button access through L1 plus the left
stick.

The feature must be controller-agnostic, preserve existing configurations by
default, and create a clear behavioral contract that can later be reproduced in
a RetroArch core or input layer. This phase does not modify or control the AYN
Thor and does not change RetroArch settings.

## Existing State

The supplied `shipofharkinian.json` already maps SDL axes 0/1 to the N64 analog
stick and axes 2/3 to Shipwright's additional right stick. Free Look and Right
Stick Aiming are enabled. Its physical layout currently maps A/B/X/Y to the four
C-buttons, R1 to N64 A, R2 to N64 B, L2 to N64 Z, L1 to N64 R, and View/Back to
N64 L.

Shipwright's current mapping factories accept one physical input and one
virtual output. M1 and M2 are valid virtual buttons, but no mapping schema field
can make another mapping conditional on either modifier. Editing JSON alone
therefore cannot implement the requested chord or suppress analog movement
while it is active.

## Approaches Considered

### 1. Post-process the completed controller sample (recommended)

After normal button and stick mappings and simulated input lag have populated
an `OSContPad` sample, run a small generic transform before `PadMgr` derives
press/release state. The transform receives configuration plus a small latch state, detects
the configured modifier, converts the dominant left-stick direction to a
configured button mask, and zeros the analog stick while active.

This is the smallest behaviorally complete change. It preserves every existing
mapping class. The modifier and stick are delayed together before the generated
C-button is derived, so their timing remains synchronized.

### 2. Add conditional variants of every mapping class

New SDL, keyboard, and mouse mapping classes could each accept required and
forbidden modifier masks. This is more general, but it expands serialization,
factories, input-editor UI, identifiers, and migration logic far beyond the one
requested behavior.

### 3. Add unconditional stick-to-button mappings in JSON

The existing factories can map stick directions directly to buttons, but they
cannot condition those mappings on L1/L2. Analog movement and C-buttons would
fire together during ordinary play. This does not meet the requirement.

## Input Transform

Implement a pure, independently testable transform with these inputs:

- current `OSContPad` sample;
- enabled flag;
- modifier bitmask, defaulting to M1;
- output masks for up, down, left, and right, defaulting to C-Up, C-Down,
  C-Left, and C-Right;
- activation and release thresholds;
- previous latched direction.

Behavior:

1. When disabled or the modifier is not held, reset the latch and preserve the
   sample byte-for-byte.
2. When the modifier is held but the stick is below the activation threshold,
   suppress analog movement and emit no C-button.
3. Once activated, choose exactly one direction using the dominant absolute
   axis. Equal diagonals use a documented stable priority rather than emitting
   two item buttons.
4. Keep the selected direction active until its axis falls below the lower
   release threshold. This hysteresis prevents rapid on/off chatter.
5. Permit a direction change when another axis becomes dominant and crosses the
   activation threshold.
6. While the chord is active, zero both analog components so Link cannot move
   while selecting a C-button.
7. Preserve all unrelated buttons, right-stick values, gyro values, and the
   modifier bit itself.

The transform runs after the controller's simulated-input-lag buffer and before
`PadMgr_ProcessInputs`. The modifier and stick therefore share the same delayed
sample, and the generated C-button participates normally in press/release state.

## Configuration and UI

Add a disabled-by-default setting under controller controls:

`Modifier + Left Stick C-Buttons`

The setting selects M1 or M2, with M1 as the default. Activation and release
thresholds remain internal conservative defaults for the initial release; they
are covered by tests and can be exposed later only if real hardware evidence
shows that tuning is necessary.

The input editor continues to control which physical button maps to M1/M2. No
physical controller identity or AYN Thor button number is hard-coded.

For the supplied configuration, the intended profile is:

| Physical control | Shipwright input |
| --- | --- |
| Left stick | Analog stick; C-buttons while L1 is held |
| Right stick | Additional stick for Free Look and aiming |
| A | N64 A |
| B | N64 B |
| L2 | N64 Z |
| R1 | N64 R |
| L1 | M1 |
| View/Back | N64 L |
| Start | N64 Start |
| D-pad | N64 D-pad |

X and Y may remain unmapped or serve as redundant C-button shortcuts; they are
not required for the modifier layer.

## Compatibility and Persistence

- The feature is disabled by default, so existing users and upstream behavior
  do not change.
- Existing controller JSON remains valid; new settings are additive.
- The package name and persistent signing identity do not change.
- The APK versionName, versionCode, Git tag, and filename advance together
  using the established numeric convention.
- No ROM, user-derived OTR/O2R, texture pack, save, or device configuration is
  embedded. The normal project-generated `assets/soh.o2r` remains required.

## Test Contract

Tests must demonstrate the red-green cycle and cover:

- disabled pass-through;
- modifier-not-held pass-through;
- each of the four cardinal C-button outputs;
- analog suppression while modified, including the modifier-only neutral case;
- dominant-axis selection for diagonals and stable equal-axis priority;
- activation/release hysteresis;
- direction switching;
- release resetting the latch;
- preservation of unrelated buttons, right stick, and gyro;
- transform application before the simulated-input-lag buffer;
- settings/UI registration and Android contract expectations.

After unit and contract tests pass, build the arm64-v8a release APK, normalize
and verify it with the existing persistent signing worker, verify 16 KiB native
alignment and payload exclusions, publish the numeric release to the existing
public bleeding-edge repository, then download and verify the published APK.
Do not install it or modify the Thor in this phase.

## RetroArch Follow-Up

The portable behavioral contract is: normal left analog movement; while a
chosen modifier is held, suppress analog output and emit one dominant C-button
with hysteresis. A later read-only assessment will identify the exact RetroArch
core and configuration in use and determine whether its remap format can express
that contract. If not, the equivalent must live in the core/input layer rather
than abusing RetroArch's global hotkey modifier. No device mutation is included
in this design.
