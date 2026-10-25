# Windows-native Android Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify the complete SOH Android unsigned candidate on Windows without Docker, then remove the obsolete SOH container implementation and local resources.

**Architecture:** A tested PowerShell orchestrator performs an isolated short-path clone, pinned Windows host-tool build, semantic `soh.o2r` validation, Android Gradle build, and atomic artifact export. GitHub Actions invokes the same clone mode under `RUNNER_TEMP\w` on `windows-2022` to stay below Ninja's Windows path ceiling; persistent signing remains outside this pipeline.

**Tech Stack:** PowerShell 7, Pester, Git, Visual Studio 2022/v143, CMake 3.31.5, vcpkg 2026.07.29, Android SDK/NDK 26, Gradle, GitHub Actions.

---

### Task 1: Specify the PowerShell build contract with failing tests

**Files:**
- Create: `scripts/android/tests/windows-native-build.Tests.ps1`
- Modify: `scripts/android/tests/android-ci.Tests.ps1`

- [x] Assert that the builder exposes isolated/current-checkout modes, validates clean committed source, pins vcpkg, uses `GenerateSohOtr`, rejects forbidden archive payloads, compares archive content manifests, runs Android tests and assembly, and cleans its working root.
- [x] Change CI expectations from Linux/container commands to `windows-2022` and the Windows builder.
- [x] Run the two test files and confirm failure because the builder and workflow are not yet implemented.

### Task 2: Implement the Windows-native builder

**Files:**
- Create: `scripts/android/Build-WindowsAndroidRelease.ps1`

- [x] Implement prerequisite resolution for Git, Visual Studio/v143, Android SDK, CMake, Java, and Python.
- [x] Implement a non-linked committed-source clone under a unique D-drive working directory for local use and an explicit current-checkout mode for CI.
- [x] Clone and detach the pinned vcpkg commit inside the working root.
- [x] Configure and build `GenerateSohOtr` with the Visual Studio generator.
- [x] Validate the generated archive and optional reference content-manifest equality, then copy it into Android assets.
- [x] Generate temporary Android SDK properties, run unit tests plus release assembly, validate one unsigned arm64 candidate, export it atomically, and clean the working root.
- [x] Run the focused tests and confirm they pass.

### Task 3: Prove a clean local build

**Files:**
- Modify after evidence: `docs/android/VALIDATION.md`

- [x] Invoke the builder from the clean committed checkout using `D:\Temp`, with the current container-generated `soh.o2r` as the semantic reference.
- [x] Confirm host asset generation, manifest equality, Android tests, native compilation, APK assembly, ABI/payload checks, and output hashes.
- [x] Confirm the permanent checkout did not acquire build directories or change ignored generated assets.

### Task 4: Replace CI and remove repository Docker support

**Files:**
- Modify: `.github/workflows/android.yml`
- Modify: `scripts/android/verify-feature-parity.ps1`
- Modify: `scripts/android/tests/android-ci.Tests.ps1`
- Modify: `docs/android/FEATURE_PARITY.md`
- Modify: `docs/android/PROVENANCE.md`
- Modify: `docs/android/THREE_SOURCE_PARITY_CHECKLIST.md`
- Modify: `docs/android/VALIDATION.md`
- Delete: `Dockerfile`
- Delete: `docker/`

- [x] Wire `windows-2022`, recursive checkout, Android SDK provisioning, the Windows builder, and unsigned artifact upload.
- [x] Replace container parity markers with the Windows-native build contract.
- [x] Label the original container validation as historical and record the clean Windows evidence.
- [x] Add a test rejecting active Docker implementation references, then remove all active references and files.
- [x] Run the complete Android Pester and native contract suites.

### Task 5: Publish and round-trip CI

**Files:**
- Commit all verified source and documentation changes.

- [ ] Push the existing branch to `bleeding-edge/main`.
- [ ] Wait for the Android GitHub Actions run to complete successfully.
- [ ] Download its unsigned APK artifact and verify SHA-256, package, version, arm64 ABI, `assets/soh.o2r`, and forbidden-payload absence.

### Task 6: Remove exact local SOH Docker resources

- [ ] Reconfirm the image and two volumes are unused and retain their pre-removal byte counts.
- [ ] Remove `djvdw7f2ro/android-builder-soh:v1.0.4`, `android_builder_soh_buildtools`, and `android_builder_soh_gradle` only.
- [ ] Run Docker disk accounting again and report the exact reclaimed SOH storage without pruning unrelated Docker state.
