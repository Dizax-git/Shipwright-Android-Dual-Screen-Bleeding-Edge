# Android Dual-Display Immersive Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make both Shipwright Android display windows fill their displays vertically while keeping Android system bars hidden in immersive mode.

**Architecture:** A single package-private helper owns system-bar policy for any Android `Window`. The main SDL Activity and secondary Presentation call it from creation, resume/start, and focus restoration paths so neither window depends on a one-time SDL callback.

**Tech Stack:** Java, Android Views/Window APIs, AndroidX Core, Pester source-contract tests, Gradle Android unit/build tasks.

---

### Task 1: Add the failing lifecycle contract

**Files:**
- Create: `scripts/android/tests/immersive-fullscreen.Tests.ps1`

- [ ] Write a Pester test requiring a shared `ImmersiveMode.apply(Window)` helper, Activity calls after creation/resume/focus gain, and Presentation calls during creation/start/focus gain.
- [ ] Run `Invoke-Pester .\scripts\android\tests\immersive-fullscreen.Tests.ps1 -Output Detailed` and confirm it fails because the helper and calls do not exist.

### Task 2: Implement the shared immersive policy

**Files:**
- Create: `Android/app/src/main/java/com/dishii/soh/ImmersiveMode.java`
- Modify: `Android/app/src/main/java/com/dishii/soh/MainActivity.java`
- Modify: `Android/app/src/main/java/com/dishii/soh/DualScreenHudPresentation.java`

- [ ] Add `ImmersiveMode.apply(Window)` using `WindowCompat.setDecorFitsSystemWindows(window, false)`, `WindowInsetsControllerCompat.hide(WindowInsetsCompat.Type.systemBars())`, transient-bars-by-swipe behavior, fullscreen window flags, and the existing legacy immersive UI bitmask.
- [ ] Invoke it from `MainActivity.onCreate`, `onResume`, and `onWindowFocusChanged(true)`.
- [ ] Invoke it from `DualScreenHudPresentation.onCreate`, `onStart`, and `onWindowFocusChanged(true)`.
- [ ] Run the focused Pester test and `Android\gradlew.bat --no-daemon :app:testReleaseUnitTest` and confirm both pass.

### Task 3: Verify and publish the release

**Files:**
- Modify: `Android/version.properties`
- Modify: `docs/android/THREE_SOURCE_PARITY_CHECKLIST.md`
- Modify: `docs/android/FEATURE_PARITY.md`

- [ ] Advance the Android release identity monotonically for the new APK.
- [ ] Run all Pester Android tests, Android unit tests, feature-parity verification, and the release build.
- [ ] Normalize/sign the verified APK with the existing persistent `com.dishii.soh` identity, verify package/version/ABI/alignment/signer/forbidden payloads, and publish atomically to the established GitHub release feed.
- [ ] Do not install, launch, send input, or change device settings. If connected, use ADB only for read-only state/log evidence.
- [ ] Commit the tested source and release metadata, push the existing public branch, create the release, then independently download and verify the published asset.
