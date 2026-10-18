# Android Feature Parity

This fork composes three source contracts rather than treating any one Android tree as a complete replacement:

| Source | Pinned source | Preserved contract | Current evidence |
|---|---|---|---|
| HarbourMasters/Shipwright | `9.3.0` at `ecd889c2019b87e78b0a3d942a6a3fe98b7efaed` | Complete stable game, randomizer, renderer, Torch extraction, assets, and desktop behavior | Exact stable commit is an ancestor of this branch. The carried source diff is audited separately from the official gameplay tree; the now-official language and talk-hook behavior is preserved. |
| Waterdish/Shipwright-Android | `main` at `c9d8f4a1219d4db0b577a050634679f72f7b3595` | Android 7+ application, setup and migration, shared `SOH` storage, mods, system ROM picker, touch controller, free-look/aiming, rumble, controller Back handling, controller database, keep-awake behavior, multi-ABI debug build, and arm64 release build | `verify-feature-parity.ps1`, Android unit tests, the Windows-native build contract, and Android CI check the adapted platform implementation. Raw `.z64`, `.zip`, and `.7z` import retains supported-hash validation. Official Torch YML metadata replaces ZAPD XML; version-aware refresh touches only APK-owned metadata and `soh.o2r`, not user ROMs or saves. The obsolete reference container is deliberately not retained. |
| chilhaps/Shipwright-Android-Dual-Screen | `main` at `33185ad59e011959aa6a6fbcf4a854f123b7e4a3` | Presentation-display discovery, secondary native surface, shared-context HUD renderer, 4:3 HUD presentation, gameplay HUD routing, menu toggle, and primary targeting/enemy-health rendering | Java lifecycle tests, native/routing contract tests, and `verify-feature-parity.ps1` cover the port against the maintained renderer API. |

## Thor composition

The AYN Thor has a landscape primary gameplay display and a presentation-capable lower touch display. When that display is present, the native HUD surface and the complete Waterdish touch-controller overlay share the lower presentation: the HUD surface is the background and the controller is the interactive layer above it. The upper display therefore remains clean gameplay and menu output. If the lower display disappears, the same controller view is reattached to the primary activity immediately; its visibility preference and eye toggle are preserved across moves.

The **Dual-Screen HUD** option controls native HUD routing. It does not remove the touch controller. Hiding the controller with its eye button hides only the interactive overlay, allowing the lower display to remain a HUD-only panel for physical-controller play.

The optional per-port modifier-stick layer converts M1 or M2 plus the left stick into the four N64 C-buttons before `PadMgr` derives button edges. The same sample's analog movement is suppressed while the modifier is held; unrelated buttons, right-stick camera input, and gyro input are preserved. `Set-TraditionalControllerLayout.ps1` provides a tested Xbox-style mapping with L1 as M1, L2 as Z, R1 as R, View as L, and the right stick reserved for free-look camera and aiming.

Both Android windows use the same immersive-mode policy. The main SDL Activity restores it after creation, resume, and focus gain; the lower-display Presentation restores it after creation, start, and focus gain. This prevents either window lifecycle from leaving the upper game surface inset by Android system bars.

## Release identity

`Android/version.properties` is the single build manifest. The APK uses a numeric four-component `versionName` for broad updater compatibility and a monotonic `versionCode` encoded as:

`major * 100000000 + minor * 1000000 + patch * 1000 + portRevision`

The release tag is the same numeric four-component value as Android `versionName`. `Get-AndroidReleaseVersion.ps1`, Gradle, and `verify-apk.ps1` reject inconsistent names, codes, tags, or release artifacts before publication.

## Repeatable audit

Run:

```powershell
.\scripts\android\verify-feature-parity.ps1 `
  -Repository . `
  -StableRef 9.3.0 `
  -WaterdishRef waterdish-reference/main `
  -DualScreenRef dual-screen-reference/main
```

The audit must report `PASS` for the official base, Waterdish Android layer, Chilhaps dual-screen layer, Thor composition, and release version. Runtime placement remains a separate AYN Thor verification gate; source parity alone does not prove physical-display behavior.
