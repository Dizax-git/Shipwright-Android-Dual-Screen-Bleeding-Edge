# Windows-native Android pipeline design

## Objective

Replace the SOH Android container build with one reproducible Windows-native pipeline that generates `soh.o2r`, compiles and tests the arm64 Android application, and emits the unsigned APK consumed by the existing persistent-signing workflow. After both local and GitHub validation pass, remove the obsolete repository container implementation and only the locally installed Docker resources attributable to SOH.

## Build architecture

`scripts/android/Build-WindowsAndroidRelease.ps1` is the single entry point. A local invocation rejects a dirty repository and creates a full, non-linked clone of the committed source under a unique `D:\Temp` working root. GitHub Actions uses its already-isolated checkout through an explicit switch. This keeps CMake, vcpkg, Gradle, `.cxx`, and APK build products away from the permanent checkout without symlinks.

The host stage uses Visual Studio 2022, v143, the Windows SDK, CMake 3.31.5, and vcpkg tag `2026.07.29` at commit `c76c06644034521fb761a39f8f52d8e87d1103d5`. It builds only `GenerateSohOtr`; that target invokes ZAPD with `--norom`, so no user ROM is read or required. The resulting archive is opened as ZIP, checked for entries and forbidden game/save payloads, optionally compared with a reference archive by a deterministic content-manifest hash, and copied into Android assets.

The Android stage writes a temporary `local.properties`, uses a working-root Gradle home, and runs `:app:testReleaseUnitTest :app:assembleRelease`. It requires exactly one arm64 unsigned APK with `assets/soh.o2r`, copies that candidate to the requested output directory, reports SHA-256 evidence, and removes the entire working root unless an explicitly named diagnostic preservation switch is used.

## CI and release boundary

`.github/workflows/android.yml` moves to `windows-2022`, installs the pinned Android SDK packages, invokes the same PowerShell entry point against the checked-out source, and uploads exactly one unsigned candidate. It never receives signing credentials or publishes a release. Persistent signing and release publication remain separate existing operations.

## Repository cleanup

After a clean local end-to-end build passes, delete the root `Dockerfile`, `docker/`, container-specific parity requirements, and historical wording that claims a container is the maintained implementation. Historical validation records may identify the old image as historical evidence but must label it superseded. A repository-wide contract test rejects active Docker build references.

After the pushed GitHub workflow passes, remove only these local Docker objects:

- image `djvdw7f2ro/android-builder-soh:v1.0.4`
- volume `android_builder_soh_buildtools`
- volume `android_builder_soh_gradle`

Do not prune unrelated images, containers, volumes, or shared builder caches.

## Verification

- Pester tests prove prerequisite discovery, clean-checkout enforcement, archive-content hashing, forbidden-payload rejection, CI wiring, D-drive isolation, and absence of active Docker build references.
- A clean local invocation regenerates `soh.o2r`, compares it with the known container-generated archive by content manifest, runs Android unit tests, and assembles the APK.
- Existing Android parity and APK contract tests remain green.
- The pushed GitHub Actions run completes and its uploaded APK is downloaded and hash/contents checked.
- Docker disk accounting before and after proves the exact SOH resources removed and the amount reclaimed.
