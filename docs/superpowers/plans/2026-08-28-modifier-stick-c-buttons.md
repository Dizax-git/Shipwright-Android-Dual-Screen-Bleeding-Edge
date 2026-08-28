# Modifier + Left Stick C-Button Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an optional M1/M2 + left-stick C-button layer, apply the approved traditional mapping to the supplied settings JSON, and publish a verified persistently signed Android release.

**Architecture:** A header-only pure transform converts one completed `OSContPad` sample before it enters the existing input-lag buffer. The transform is configured per controller port, while a tested PowerShell updater rewrites only SDL controller bindings in an external Shipwright JSON file. Existing mappings remain unchanged unless the feature or updater is explicitly used.

**Tech Stack:** C++17, libultraship controller pipeline, ImGui/CVars, Pester 5, Python-assisted host C++ test compilation, Gradle/NDK Android build, persistent local APK signing worker, GitHub Releases.

---

### Task 1: Pure Modifier-Stick Transform

**Files:**
- Create: `libultraship/include/libultraship/controller/ModifierStickCButtons.h`
- Create: `scripts/android/tests/native/modifier-stick-c-buttons-test.cpp`
- Create: `scripts/android/run-modifier-stick-tests.py`
- Modify: `.github/workflows/android.yml`

- [ ] **Step 1: Write the failing native behavior test**

Cover disabled and modifier-free pass-through, each cardinal output, neutral suppression, dominant-axis selection, equal-axis priority, hysteresis, switching, reset, and unrelated-field preservation. The test calls this wished-for API:

```cpp
ModifierStickState state{};
ModifierStickConfig config{true, 0x0040, 45, 30};
ModifierStickSample sample{0x0040, 70, 0};
ApplyModifierStickCButtons(sample, state, config);
assert((sample.buttons & BTN_CRIGHT) != 0);
assert(sample.stickX == 0 && sample.stickY == 0);
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```powershell
python scripts/android/run-modifier-stick-tests.py --build-dir D:\Temp\shipwright-modifier-stick-tests
```

Expected: compilation fails because `ModifierStickCButtons.h` does not exist.

- [ ] **Step 3: Implement the minimal pure transform**

Define `ModifierStickDirection`, `ModifierStickState`, `ModifierStickConfig`, `ModifierStickSample`, and `ApplyModifierStickCButtons`. Use one dominant direction, stable horizontal priority for equal magnitudes, activation threshold 45, release threshold 30, and preserve every field not owned by the transform.

- [ ] **Step 4: Run the native test and verify GREEN**

Run the same command. Expected: the compiled test exits `0` and prints `modifier-stick-c-buttons: PASS`.

- [ ] **Step 5: Add the native test to Android CI**

Before Gradle, run:

```yaml
- name: Test native modifier-stick behavior
  run: python3 scripts/android/run-modifier-stick-tests.py --build-dir build-native-tests
```

- [ ] **Step 6: Commit**

```powershell
git add libultraship/include/libultraship/controller/ModifierStickCButtons.h scripts/android .github/workflows/android.yml
git commit -m "test(android): define modifier-stick C-button behavior"
```

### Task 2: Controller Pipeline and Settings UI

**Files:**
- Modify: `libultraship/include/libultraship/controller/controldevice/controller/Controller.h`
- Modify: `libultraship/src/libultraship/controller/controldevice/controller/Controller.cpp`
- Modify: `soh/soh/Enhancements/controls/SohInputEditorWindow.cpp`
- Modify: `scripts/android/tests/controller-modifier-stick.Tests.ps1`

- [ ] **Step 1: Write the failing source contract test**

Require the controller to apply the transform before `mPadBuffer.push_front`, maintain one state per controller, and expose an enable toggle plus M1/M2 selector in the controller UI.

- [ ] **Step 2: Run the Pester test and verify RED**

```powershell
Invoke-Pester scripts/android/tests/controller-modifier-stick.Tests.ps1 -Output Detailed
```

Expected: failures for the absent pipeline call and UI controls.

- [ ] **Step 3: Integrate the transform**

Read per-port CVars:

```text
gSettings.Controllers.Port1.ModifierStickCButtons.Enabled
gSettings.Controllers.Port1.ModifierStickCButtons.Modifier
```

Translate modifier choice `1` to M1 (`0x0040`) and `2` to M2 (`0x0080`). Apply the transform after both sticks and buttons are populated and before the sample is buffered.

- [ ] **Step 4: Add the controller UI**

Under `Modifier Buttons`, add `Modifier + Left Stick C-Buttons` and an M1/M2 selector. Keep the feature disabled by default.

- [ ] **Step 5: Run native and Pester tests and verify GREEN**

Run both commands from Tasks 1 and 2. Expected: all pass.

- [ ] **Step 6: Commit**

```powershell
git add libultraship soh scripts/android/tests/controller-modifier-stick.Tests.ps1
git commit -m "feat(android): add modifier-stick C-button layer"
```

### Task 3: Traditional Settings Updater

**Files:**
- Create: `scripts/android/Set-TraditionalControllerLayout.ps1`
- Create: `scripts/android/tests/traditional-controller-layout.Tests.ps1`
- Modify: `README.md`

- [ ] **Step 1: Write failing fixture-based Pester tests**

Copy the supplied mapping shape into a temporary fixture. Assert that the updater preserves keyboard mappings, sticks, rumble, unrelated CVars, and JSON validity while producing:

```text
A -> physical A
B -> physical B
Z -> L2
R -> R1
L -> View/Back
Start -> Start
M1 -> L1
D-pad -> physical D-pad
```

Also require the modifier-stick feature to be enabled for Port 1 and require idempotence on a second run.

- [ ] **Step 2: Run the Pester test and verify RED**

```powershell
Invoke-Pester scripts/android/tests/traditional-controller-layout.Tests.ps1 -Output Detailed
```

Expected: failure because the updater does not exist.

- [ ] **Step 3: Implement the updater**

Accept `-Path`, parse JSON, replace only Port 1 SDL button mappings, retain keyboard mapping IDs, atomically write UTF-8 JSON beside the original only after validation, and do not create an untracked backup.

- [ ] **Step 4: Run the Pester test and verify GREEN**

Run the same command. Expected: all assertions pass, including idempotence.

- [ ] **Step 5: Apply it to the supplied file**

```powershell
scripts/android/Set-TraditionalControllerLayout.ps1 -Path "G:\Modding\Downloads\Ship of Harkinian\shipofharkinian.json"
```

Parse the final JSON independently and report its SHA-256. Do not touch the device copy.

- [ ] **Step 6: Document usage and commit**

```powershell
git add scripts/android/Set-TraditionalControllerLayout.ps1 scripts/android/tests/traditional-controller-layout.Tests.ps1 README.md
git commit -m "feat(android): add traditional controller layout updater"
```

### Task 4: Version, Full Verification, and APK Build

**Files:**
- Modify: `Android/version.properties`
- Modify: `scripts/android/tests/android-release-version.Tests.ps1`
- Modify: `scripts/android/tests/verify-apk.Tests.ps1`
- Modify: `docs/android/PROVENANCE.md`
- Modify: `docs/android/THREE_SOURCE_PARITY_CHECKLIST.md`
- Modify: `docs/android/VALIDATION.md`

- [ ] **Step 1: Advance the numeric version**

Set `Android/version.properties` to `versionName=9.2.3.5`,
`versionCode=902003005`, `portRevision=5`, and `releaseTag=9.2.3.5`.
Update exact-version test fixtures to match.

- [ ] **Step 2: Run all host-side contracts**

Run the native test, all `scripts/android/tests/*.Tests.ps1`, `verify-feature-parity.ps1`, `verify-provenance.ps1`, and source formatting checks. Expected: zero failures.

- [ ] **Step 3: Build the unsigned arm64-v8a APK**

Use the repository's established Gradle/NDK release build without installing it. Expected: one unsigned release APK containing exactly arm64-v8a.

- [ ] **Step 4: Normalize and verify persistent signing**

Use policy `shipwright` with the existing unsigned-input gate. Expected package `com.dishii.soh`, signer `5db60116997ae5cf8ea0f26fee6c0a150a8045a7cbfc10282892ff10c0348004`, exactly arm64-v8a, 16 KiB native alignment, the required project-generated `assets/soh.o2r`, and no ROM, user-derived OTR/O2R, save, or texture-pack payload.

- [ ] **Step 5: Commit verified version and provenance**

```powershell
git add -A
git commit -m "release(android): prepare dual-screen 9.2.3.5"
```

### Task 5: Public Release and Round-Trip

**Files:**
- Existing repository: `Darkaxt/Shipwright-Android-Dual-Screen-Bleeding-Edge`

- [ ] **Step 1: Push the completed main branch**

Push only after all source, build, signing, and payload checks pass.

- [ ] **Step 2: Create release `9.2.3.5`**

Create one normal public release with exactly one friendly APK and matching `.sha256` sidecar. Describe the modifier-stick behavior, traditional settings updater, signer, ABI, alignment, and absence of game data.

- [ ] **Step 3: Download and verify the release**

Download both assets into a unique D: temporary directory, verify the sidecar, rerun the signing worker's `Verify`, and confirm the GitHub release API exposes a normal APK asset.

- [ ] **Step 4: Clean disposable staging**

Remove only the exact D: build and round-trip roots after publication succeeds. Preserve the permanent checkout, signing credentials, supplied JSON, and published release.

- [ ] **Step 5: Report the deferred RetroArch phase**

Record that no device or RetroArch setting was modified. The later phase must first identify the installed core and determine whether its remap format supports conditional analog layers.
