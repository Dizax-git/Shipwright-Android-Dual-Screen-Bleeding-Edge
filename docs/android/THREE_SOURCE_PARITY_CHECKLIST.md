# Three-source parity checklist

This is the completion ledger for the Android dual-screen port. A checked item has a concrete implementation and host-side evidence. An unchecked item is either a confirmed gap or still lacks evidence strong enough to claim parity.

Source pins:

- HarbourMasters/Shipwright `9.2.3`: `cb71e22a79bc5d1f688fa881795bbd93094895fc`
- Waterdish/Shipwright-Android `main`: `c9d8f4a1219d4db0b577a050634679f72f7b3595`
- chilhaps/Shipwright-Android-Dual-Screen `main`: `33185ad59e011959aa6a6fbcf4a854f123b7e4a3`

## Level 1: HarbourMasters/Shipwright 9.2.3

- [x] The exact official stable commit is an ancestor of the maintained branch.
- [x] Official 9.2.3 gameplay, randomizer, renderer, assets, menus, network features, and desktop behavior remain the base rather than the older Android forks' 9.0.2 snapshot.
- [x] Every non-Android divergence imported from the modern Android reference is classified; `excluded-divergence` paths are byte-equivalent to official 9.2.3.
- [x] Android-compatible OTRExporter, ZAPDTR, and libultraship commits are pinned and recorded without replacing the official game tree with development/nightly Shipwright code.
- [x] The generated `soh.o2r` belongs to the official 9.2.3 source graph and the APK contains no ROM, OTR, save, state, or quest payload.

Evidence: `verify-overlay.ps1`, `verify-provenance.ps1`, `PROVENANCE.md`, and the stable-ancestor gate in `verify-feature-parity.ps1`.

## Level 2: Waterdish/Shipwright-Android

### Application and storage

- [x] Android application project, SDL activity, OpenGL ES build, Android 7+ minimum SDK, and landscape activity behavior.
- [x] First-run setup blocks native startup until required files exist.
- [x] Shared `/SOH` storage, migration from app-private storage, save preservation, and a persistent `mods` folder.
- [x] Missing or version-stale packaged assets are recopied without deleting user saves or mods.
- [x] System document picker and in-app OTR extraction flow.
- [x] Every ROM hash supported by the maintained official extractor is packaged as canonical metadata.
- [x] Raw `.z64` selection remains supported; `.zip` and `.7z` selections extend it by hash-validating extracted candidates.
- [x] Archive import iterates all `.z64` entries and accepts exactly one supported hash without relying on compressed size.

### Input and mobile behavior

- [x] Complete Waterdish touch overlay and visibility/eye-button preference.
- [x] Preserve Waterdish C-button gameplay semantics while retaining the newer direct menu-navigation injection. The directional touch cluster now emits Waterdish's right-stick C-button axes and independently injects direct menu-navigation keys.
- [x] Left analog stick, A/B/X/Y, L/R/Z, Start, and overlay hide/show behavior.
- [x] Controller Back/Select opens the Enhancements menu, including after activity transitions.
- [x] Ignore Android pseudo-gamepads such as fingerprint sensors while retaining the touch virtual controller. Known `uinput-*` sensor names are rejected at SDL's Android input-device boundary before native joystick registration.
- [x] Controller add/remove/reconnect refresh behavior is retained by the modern libultraship implementation.
- [x] Device gyroscope support is carried by the Android-compatible libultraship layer and declared optional in the manifest.
- [x] Touch free-look camera input and its in-game setting.
- [x] First-person touch action pulse/hold behavior.
- [x] Controller/device rumble with handset fallback and bounded vibration duration.
- [x] Keep-screen-awake behavior.
- [x] Android GUI scaling and controller-mapping window usability are retained through the modern libultraship Android scaling path.
- [x] Waterdish Android collision-rounding/pushblock behavior is retained where still required on the 9.2.3 base.
- [x] Waterdish's Android near-plane/depth workaround is retained in the current Prism GLES vertex shader, with CPU near clipping still disabled.
- [x] `gamecontrollerdb.txt` is packaged.

### Build and delivery

- [x] Four-ABI debug build and arm64-v8a release build, matching the final Waterdish build-type contract.
- [x] Reproducible container build scripts.
- [x] CI checks out submodules, builds `soh.o2r`, runs Android unit tests, assembles an unsigned release candidate, and uploads it as an artifact.
- [x] Release signing is external to Gradle and uses the persistent per-package identity.

Evidence: Android unit tests, `verify-feature-parity.ps1`, `verify-apk.ps1`, signing-worker verification, and the source paths cited in `FEATURE_PARITY.md`.

## Level 3: chilhaps/Shipwright-Android-Dual-Screen

- [x] Presentation-capable non-default display discovery.
- [x] Display add/remove/change listener and idempotent start/stop lifecycle.
- [x] Secondary `Presentation` with a native `SurfaceView` and JNI surface lifecycle.
- [x] Secondary EGL surface/context sharing the primary renderer's HUD framebuffer texture.
- [x] Dedicated 320x240 HUD framebuffer and 4:3 aspect correction.
- [x] HUD framebuffer is cleared at the start of a HUD frame to prevent stale elements.
- [x] Core gameplay HUD redirection leaves the 3D world and menus on the primary display.
- [x] Keep Z-target reticle and enemy health bar on the primary display without clearing already-rendered secondary HUD elements. The HUD pass now pauses and resumes its framebuffer target without starting a second clearing pass.
- [x] Present the completed HUD once per rendered frame and restore primary EGL bindings.
- [x] Secondary surface is destroyed on pause, display removal, and activity destruction.
- [x] `Dual-Screen HUD` setting is present under Graphics/UI and defaults on when an eligible display exists.
- [x] A failed or missing secondary surface falls back to normal single-screen HUD rendering.

### Thor composition

- [x] Primary display hosts gameplay and menus.
- [x] Secondary display layers the native HUD surface below the complete touch-controller overlay.
- [x] Removing/dismissing the secondary presentation reparents the same overlay to the primary display.
- [x] The eye button hides only touch controls; it does not disable the secondary HUD.
- [ ] Physical two-display behavior remains unclaimed until separately authorized runtime validation; host completion must not use ADB as a substitute.

Evidence: Java lifecycle tests, native routing tests, controller-overlay routing tests, `waterdish-mobile-parity.Tests.ps1`, and `verify-feature-parity.ps1`.

## APK release identity

- [x] `Android/version.properties` is the single version source.
- [x] Numeric updater-safe `versionName=9.2.3.3`.
- [x] Monotonic `versionCode=902003003`, greater than all prior `.1` and `.2` APK codes.
- [x] Descriptive release tag `v9.2.3-dual-screen.3` is validated against the version manifest.
- [x] Gradle output naming, manifest metadata, APK verifier expectations, and release tag derive from the same manifest.
- [x] Rebuild after all host-side parity gaps close and verify the final signed APK against package, version, ABI, alignment, signer, and forbidden-payload gates.
- [x] Publish exactly the verified signed artifact and checksum.
- [x] Verify the published GitHub release metadata and downloadable APK expose the new version identity to updater clients.

## Completion rule

Three-level parity is complete only when every item above is checked, all host tests and the full release build pass, the signed APK passes package/version/ABI/alignment/signer/payload checks, and the public release asset is independently downloaded and reverified. Device operations are outside this checklist unless explicitly authorized.
