# Android validation record

## Stable single-screen baseline

Validated on 2026-08-21 before applying the dual-screen feature layer.

| Check | Result |
|---|---|
| Official release | HarbourMasters/Shipwright `9.2.3` |
| Official commit | `cb71e22a79bc5d1f688fa881795bbd93094895fc` |
| Android OTRExporter | `77d47bc6d1767edb56f7cd461c5e1b7681f36a93` |
| Android ZAPDTR | `1aa5bfc16cba8e487077a2a1f5ab7d5263ecf508` |
| Android libultraship | `3a9af4fc67ddc2d476f204fdc43f87dc363ee0e0` |
| Build image | `djvdw7f2ro/android-builder-soh:v1.0.4` (`sha256:73d44429eefdd78cecf61bcfd3ecb83c632b51bc0d674a516b5a6f6126464846`) |
| Build command | `docker/scripts/build_android_project.sh --release` followed by cached `./gradlew --no-daemon assembleRelease` |
| APK | `Android/app/build/outputs/apk/release/soh_release_v9.2.3.apk` |
| APK size | 52,656,561 bytes |
| APK SHA-256 | `d3d941d3608695a714b5b017a387b87b8f0036be9735f36b33aa966a190f5079` |
| Package | `com.dishii.soh` |
| Version | `versionName=9.2.3`, `versionCode=10` |
| ABI | exactly `arm64-v8a` |
| Alignment | `zipalign -c -P 16 -v 4` passed |
| Signature | intentionally unsigned baseline; `apksigner verify` rejected it with missing signature metadata |
| APK payload | 8,157 entries; no `.z64`, `.n64`, `.v64`, `.otr`, save, or quest payload |
| Bundled project archive | only `assets/soh.o2r`; SHA-256 `e16df30f020acbb59f436648e9f3ecc75dd5565835c0b4d95f312d98e213d0d9` |
| Nested archive payload | 1,279 entries; no ROM, OTR/O2R, save, or quest payload |

The unsigned result is the deliberate handoff boundary: release candidates are built without an ad-hoc Gradle identity and are normalized only by the persistent local signing worker after the final dual-screen APK passes its own validation gates.

## Dual-screen signed release candidate

Validated on 2026-08-21 from local commit `f15c6e898` and official Shipwright `9.2.3`.

| Check | Result |
|---|---|
| Build commands | `gradlew.bat :app:externalNativeBuildRelease`, then `gradlew.bat :app:testReleaseUnitTest :app:assembleRelease` |
| Host tests | 17 Android source-contract tests and Java release unit tests passed |
| Package | `com.dishii.soh` |
| Version | `versionName=9.2.3-dual-screen.1`, `versionCode=902031` |
| ABI | exactly `arm64-v8a` |
| Unsigned candidate | `soh_release_v9.2.3-dual-screen.1.apk` |
| Unsigned SHA-256 | `adf56a0b14954057f4c8eff3f2f74376f66098d237a25a30a4f3a0362c8d37c6` |
| Signed candidate SHA-256 | `31d0d44d94c340e6887ba13146ae04ed1a994f284c86b45f0b0c3834db55d3a6` |
| Persistent signer SHA-256 | `5db60116997ae5cf8ea0f26fee6c0a150a8045a7cbfc10282892ff10c0348004` |
| Alignment | `zipalign -c -P 16 4` passed |
| Signature | `apksigner verify --verbose --print-certs` passed with exactly one pinned signer |
| Payload | APK and nested `assets/soh.o2r` contain no ROM, OTR, save, state, or quest payload |
| Payload preservation | Every non-signature ZIP `(name, content hash, count)` entry matched across local signing |
| Negative signer test | Expected all-zero fingerprint was rejected with expected-versus-actual evidence |

AYN Thor installation and dual-display gameplay validation are recorded separately after installed-signer compatibility is checked.

## Local Native publication

Published atomically on 2026-08-21 after the signed candidate passed both the persistent signing-worker policy and the standalone APK verifier.

| Check | Result |
|---|---|
| Native path | `H:\My Drive\Roms\Native\The Legend of Zelda - Ocarina of Time - Ship of Harkinian Dual Screen\Ship of Harkinian - Dual Screen v9.2.3.apk` |
| APK count in game folder | exactly one |
| Published SHA-256 | `31d0d44d94c340e6887ba13146ae04ed1a994f284c86b45f0b0c3834db55d3a6` |
| Package | `com.dishii.soh` |
| Version | `versionName=9.2.3-dual-screen.1`, `versionCode=902031` |
| ABI | exactly `arm64-v8a` |
| Alignment | 16 KiB page alignment passed |
| Signer SHA-256 | `5db60116997ae5cf8ea0f26fee6c0a150a8045a7cbfc10282892ff10c0348004` |
| Payload | APK and nested `assets/soh.o2r` contain no ROM, OTR/O2R, save, state, or quest payload |

The public source repository does not contain the private local signing credential or the locally signed APK.

## AYN Thor installation

Installed as a new package on AYN Thor `bfa98654` on 2026-08-21. No prior `com.dishii.soh` installation or app data existed.

| Check | Result |
|---|---|
| Installation | `adb install -r` succeeded |
| Installed package | `com.dishii.soh` |
| Installed version | `versionName=9.2.3-dual-screen.1`, `versionCode=902031` |
| Installed base APK | byte-identical to the published APK; SHA-256 `31d0d44d94c340e6887ba13146ae04ed1a994f284c86b45f0b0c3834db55d3a6` |
| Installed signer | `5db60116997ae5cf8ea0f26fee6c0a150a8045a7cbfc10282892ff10c0348004` |
| First launch | Process and SDL native entry started without a Java or native crash |
| Storage permission | App-specific `MANAGE_EXTERNAL_STORAGE` permission granted, matching the first-launch system request |
| Device displays | AYN Thor exposed primary display `0` and presentation-capable `Screen-2` display `4` |
| Gameplay validation | pending; the device remained behind its secure keyguard, which ADB did not bypass |

This is installation and launch evidence only. It does not claim that ROM setup, gameplay, HUD routing, or ten-minute play have been validated on the physical displays.

## Archived ROM import release candidate

Validated on 2026-08-21 from implementation commits `e9ad65930079e94a9b81b41996f1c738fc3fe67d` through `2598051276b85c74f8488ccd84af9b9e4b3bf43e`.

| Check | Result |
|---|---|
| Supported selection formats | raw `.z64`, `.zip`, and `.7z` |
| Archive selection rule | scan every case-insensitive `.z64` entry; accept only when exactly one entry matches `docs/supportedHashes.json` |
| Safety gates | rejects traversal names, encrypted archives, oversized inputs/candidates, excessive total expansion, extreme compression ratios, no match, and multiple matches |
| Existing ROM preservation | verified candidate is flushed to `OOT.z64.pending` and atomically renamed over `OOT.z64` only after validation succeeds |
| Canonical hash asset | APK `assets/supportedHashes.json` is byte-identical to `docs/supportedHashes.json` |
| Build | clean native/test compilation exposed a missing generated-asset dependency in `lintVitalAnalyzeRelease`; a regression test was added, the dependency was corrected, and `:app:testReleaseUnitTest :app:assembleRelease` then passed through `lintVitalRelease` and APK assembly |
| Host tests | 26 Pester source/artifact-contract tests and 11 Java release unit tests passed |
| Owned archive probe | production `RomImportService` accepted `Legend of Zelda, The - Ocarina of Time (USA).7z` without modifying it |
| Imported ROM hash | SHA-1 `ad69c91157f6705e8ab06c79fe08aad47bb57ba7` (`NTSC 1.0 (US)`) |
| Package | `com.dishii.soh` |
| Version | `versionName=9.2.3-dual-screen.2`, `versionCode=902032` |
| ABI | exactly `arm64-v8a` |
| Unsigned SHA-256 | `12e1785ec21e21634e68a17b601692cce9ae4c5d6cb8209273d5cf4c70f4e363` |
| Signed SHA-256 | `13df4da1702e298e5c6e4de8917c4d22ea37ade96d1bb295dee6d5ce74c77b3b` |
| Persistent signer SHA-256 | `5db60116997ae5cf8ea0f26fee6c0a150a8045a7cbfc10282892ff10c0348004` |
| Alignment | `zipalign -c -P 16 4` passed |
| Signature | `apksigner verify --verbose --print-certs` passed with exactly one pinned signer |
| Payload | APK contains no ROM, OTR/O2R, save, state, or quest payload |
| Device validation | pending because AYN Thor `bfa98654` was not connected; no unrelated connected device was modified |
