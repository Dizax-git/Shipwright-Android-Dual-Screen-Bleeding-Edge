# Android Feature Parity

This fork composes three source contracts rather than treating any one Android tree as a complete replacement:

| Source | Pinned source | Preserved contract | Current evidence |
|---|---|---|---|
| HarbourMasters/Shipwright | `9.2.3` at `cb71e22a79bc5d1f688fa881795bbd93094895fc` | Complete stable game, randomizer, renderer, assets, and desktop behavior | The stable commit is an ancestor of this branch; excluded Android-reference divergences are checked against the stable tree by `verify-overlay.ps1`. |
| Waterdish/Shipwright-Android | `main` at `c9d8f4a1219d4db0b577a050634679f72f7b3595` | Android 7+ application, setup and migration, shared `SOH` storage, mods, system ROM picker, touch controller, free-look/aiming, rumble, controller Back handling, controller database, keep-awake behavior, container build, multi-ABI debug build, and arm64 release build | `verify-feature-parity.ps1`, Android unit tests, and the Android CI workflow check the maintained 9.2.3 implementations. Raw `.z64`, `.zip`, and `.7z` import extends the original picker without removing raw-ROM support. |
| chilhaps/Shipwright-Android-Dual-Screen | `main` at `33185ad59e011959aa6a6fbcf4a854f123b7e4a3` | Presentation-display discovery, secondary native surface, shared-context HUD renderer, 4:3 HUD presentation, gameplay HUD routing, menu toggle, and primary targeting/enemy-health rendering | Java lifecycle tests, native/routing contract tests, and `verify-feature-parity.ps1` cover the port against the maintained renderer API. |

## Thor composition

The AYN Thor has a landscape primary gameplay display and a presentation-capable lower touch display. When that display is present, the native HUD surface and the complete Waterdish touch-controller overlay share the lower presentation: the HUD surface is the background and the controller is the interactive layer above it. The upper display therefore remains clean gameplay and menu output. If the lower display disappears, the same controller view is reattached to the primary activity immediately; its visibility preference and eye toggle are preserved across moves.

The **Dual-Screen HUD** option controls native HUD routing. It does not remove the touch controller. Hiding the controller with its eye button hides only the interactive overlay, allowing the lower display to remain a HUD-only panel for physical-controller play.

The optional per-port modifier-stick layer converts M1 or M2 plus the left stick into the four N64 C-buttons before `PadMgr` derives button edges. The same sample's analog movement is suppressed while the modifier is held; unrelated buttons, right-stick camera input, and gyro input are preserved. `Set-TraditionalControllerLayout.ps1` provides a tested Xbox-style mapping with L1 as M1, L2 as Z, R1 as R, View as L, and the right stick reserved for free-look camera and aiming.

Both Android windows use the same immersive-mode policy. The main SDL Activity restores it after creation, resume, and focus gain; the lower-display Presentation restores it after creation, start, and focus gain. This prevents either window lifecycle from leaving the upper game surface inset by Android system bars.

## Release identity

`Android/version.properties` is the single build manifest. The APK uses a numeric four-component `versionName` for broad updater compatibility and a monotonic `versionCode` encoded as:

`major * 100000000 + minor * 1000000 + patch * 1000 + portRevision`

The descriptive release tag remains `v<upstream>-dual-screen.<revision>`. `Get-AndroidReleaseVersion.ps1`, Gradle, and `verify-apk.ps1` reject inconsistent names, codes, tags, or release artifacts before publication.

## Repeatable audit

Run:

```powershell
.\scripts\android\verify-feature-parity.ps1 `
  -Repository . `
  -StableRef 9.2.3 `
  -WaterdishRef waterdish-reference/main `
  -DualScreenRef dual-screen-reference/main
```

The audit must report `PASS` for the official base, Waterdish Android layer, Chilhaps dual-screen layer, Thor composition, and release version. Runtime placement remains a separate AYN Thor verification gate; source parity alone does not prove physical-display behavior.
