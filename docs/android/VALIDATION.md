# Android validation record

## Official 9.3.0 / Android 9.3.0.1 host verification (2026-10-18)

- Compiled source: `57d1a517aefca9baac449218eccb83cbb633206e`, based on official
  `9.3.0` / `ecd889c2019b87e78b0a3d942a6a3fe98b7efaed`.
  Official libultraship `62e973aeb4a53ad4d22bb91e2d9373ecdfcd246c` plus the
  versioned Android-only patch; Torch `2ab12fe9660aec04e02ee89fe81baed304a1a1d6`.
- Windows-native Torch generated the project archive; no ROM was required.
  The official Windows release ZIP matched its published SHA-256
  `bf46cfc41be17f3b63901db46dac46ac2a63ed3851aa13e3ba6c9ec5d0ac2632`.
  Both project archives contain exactly 1,056 entries: 1,054 are byte-identical;
  the HLSL and Metal shader texts differ only by CRLF/LF line endings. Android's
  OpenGL shader is byte-identical. Bundled `soh.o2r` SHA-256:
  `fb31ad1460b176a7ec3ab84469f5e909405fbb011c34be3bb8b618198713311b`.
- Full Android source contracts passed (68 cases); Java release unit XML reports
  have zero failures/errors, including versioned bundled-asset refresh. The host
  modifier-stick executable passed. Android runtime target `soh` and its SDL2
  dependency built successfully; the unrelated desktop Torch CLI is not an APK target.
- Gradle 9.8 / JDK 21 ran through the installed shared desktop gate (ticket
  `40407f41a1b642dca59bb4374362ddd0`), which recorded successful completion and
  exit code 0. The wrapper and its exact daemon PID were confirmed exited.
  Effective workers=2, no parallel projects, 3-GiB JVM budgets, one 512-MiB test
  fork; every native compiler edge used a depth-2 Ninja pool, link depth=1.
  Windows host MSBuild used `/m:2` and compiler `ProcessorNumber=1`.
- Normalized APK: `Ship.of.Harkinian.Dual.Screen.9.3.0.1.apk`, package
  `com.dishii.soh`, version `9.3.0.1`, code `903000001`, exactly `arm64-v8a`.
  SHA-256 `9b691a6c6fa13549f7e912ddf35f94c77950a91188a0414b81867a5075dd6fbe`;
  persistent signer `5db60116997ae5cf8ea0f26fee6c0a150a8045a7cbfc10282892ff10c0348004`.
  Signing policy and independent verifier passed signature, ZIP 16-KiB alignment,
  every ELF LOAD segment's 16-KiB alignment, package/version/ABI and nested-payload
  checks. Both launcher libraries (`libSDL2.so`, `libsoh.so`) are present and all
  DT_NEEDED dependencies resolve to a bundled library or an Android system library.
  Canonical Torch YAML and the exact generated project archive are packaged;
  legacy extractor XML is absent. No raw ROM, user-derived game archive, save or
  quest payload is bundled.
- No device was installed, launched or controlled. New-version device/gameplay
  validation remains pending; historical runtime evidence below is not evidence
  of this new release's runtime behavior. Existing persistent-signer installations
  can update without an identity migration.

## Stable single-screen baseline (historical container evidence)

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

This baseline records how the original Android layer was first validated. The container image and scripts are superseded and are not part of the maintained build pipeline.

## Windows-native Android pipeline

Validated on 2026-08-29 from clean local commit `2bc043b29` using the isolated Windows builder under `D:\Temp`.

| Check | Result |
|---|---|
| Entry point | `scripts/android/Build-WindowsAndroidRelease.ps1` |
| Isolation | Full non-linked committed-source clone under `D:\Temp`; permanent checkout remained free of generated build trees |
| Host asset build | Visual Studio 2022/v143 built only `GenerateSohOtr`; no ROM was read or required |
| vcpkg | `c76c06644034521fb761a39f8f52d8e87d1103d5` |
| Generated `soh.o2r` SHA-256 | `e9fe89b0eb628ed99cadec5681b082b487d1a3da62c2d4e2adef077c8dbb6870` |
| `soh.o2r` content-manifest SHA-256 | `a26c0fc1ba524d9943925cec2ad91546e3cd01772fc0d97c2193daa639924d5f` |
| Android work | `:app:testReleaseUnitTest :app:assembleRelease` |
| APK | `Shipwright-Android-Dual-Screen-Bleeding-Edge-9.2.3.5.apk`, 53,826,368 bytes |
| APK SHA-256 | `be58c6ba43b2b082c686ce178438d53d79f83ff2d5c59c61e186ca4bac1999c4` |
| Package/version | `com.dishii.soh`, `versionName=9.2.3.5`, `versionCode=902003005` |
| ABI | exactly `arm64-v8a` |
| Payload | Project-owned `assets/soh.o2r` present; raw ROM, user OTR/O2R, quest, save, and state payloads rejected |
| Container dependency | None; local and GitHub builds use the same PowerShell implementation |

The first GitHub run of the Windows workflow exposed a CI-only path-length defect: Android's nested `FetchContent` path exceeded Ninja's 260-character limit under GitHub's doubled repository checkout path. CI therefore invokes the same non-linked clone mode under the short `RUNNER_TEMP\w` root; no symlink, junction, or container workaround is used.

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

## Dual-display immersive fullscreen release candidate

Validated on 2026-08-22 from implementation commit `53736fb53` and official Shipwright `9.2.3`.

| Check | Result |
|---|---|
| Regression cycle | `immersive-fullscreen.Tests.ps1` failed 3/3 before implementation and passed 3/3 afterward |
| Full host tests | 49 Pester source/artifact-contract tests and 21 Java release unit tests passed |
| Release build | `gradlew.bat --no-daemon :app:testReleaseUnitTest :app:assembleRelease` passed through native build, lint vital, and APK assembly |
| Fullscreen policy | Shared helper applies edge-to-edge layout, hides all system bars with transient swipe reveal, and retains the legacy SDL immersive flags |
| Lifecycle coverage | Main Activity: create/resume/focus gain; secondary Presentation: create/start/focus gain |
| Read-only device evidence | Installed `.3` Activity window carried `FORCE_NOT_FULLSCREEN`; its 1920x1080 task snapshot reported 55-pixel top and bottom content insets |
| Device actions | `adb devices`, `dumpsys window`, and `dumpsys package` only; the app was not launched, focused, installed, or controlled |
| Package | `com.dishii.soh` |
| Version | `versionName=9.2.3.4`, `versionCode=902003004` |
| ABI | exactly `arm64-v8a` |
| Unsigned size / SHA-256 | 53,836,200 bytes / `79860475672e1dc101c6c9045ffb1462b2d4a8b12a63b68d72d0d7f91ff0962b` |
| Signed size / SHA-256 | 54,531,699 bytes / `e6644350a74a93eb789eda88cd64417dda7467ca27cfba4253af2612d185be35` |
| Persistent signer SHA-256 | `5db60116997ae5cf8ea0f26fee6c0a150a8045a7cbfc10282892ff10c0348004` |
| Alignment | `zipalign -c -P 16 4` passed |
| Signature | `apksigner verify --verbose --print-certs` passed with exactly one pinned signer |
| Payload | APK and nested `assets/soh.o2r` contain no ROM, OTR/O2R, save, state, or quest payload |
| Publication | Public release `v9.2.3-dual-screen.4`; normal, non-draft, non-prerelease release with exactly one APK and its checksum |
| Independent download | Downloaded through the GitHub release API into a separate staging directory; checksum sidecar, package, version, ABI, alignment, signer, and payload checks all passed |

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

## Traditional controller release candidate

Validated on 2026-08-28 from implementation commit `a1b69c4164bedf955a33ffc86fde9b8758e91127` and official Shipwright `9.2.3`.

| Check | Result |
|---|---|
| Controller behavior | Left stick remains analog movement; right stick remains camera/aim; holding modifier M1 converts the left stick to the four N64 C-buttons with dominant-axis selection and hysteresis |
| Traditional profile | A/B, Z on L2, R on R1, L on View/Back, Start, D-pad, both analog sticks, and M1 on L1 are written by `scripts/android/Set-TraditionalControllerLayout.ps1` without replacing keyboard or unrelated mappings |
| User configuration | `G:\Modding\Downloads\Ship of Harkinian\shipofharkinian.json` updated atomically; final SHA-256 `7fdb6d46da4b5d9e9a6d80a9512302ba740e5536183d4ad66e0bd28f7f7dd857` |
| Host tests | 56 Pester source/artifact-contract tests, the native modifier-stick behavior test, and 21 Java release unit tests passed with zero failures |
| Release build | `gradlew.bat --no-daemon :app:testReleaseUnitTest :app:assembleRelease` completed native build, Java tests, lint vital, and APK assembly successfully |
| Package | `com.dishii.soh` |
| Version | `versionName=9.2.3.5`, `versionCode=902003005` |
| ABI | exactly `arm64-v8a` |
| Unsigned size / SHA-256 | 53,825,608 bytes / `829ea4b21ba91ffd1c988b15b135e63dc80bfa7c58e406912d8ed8de4cc58ecf` |
| Signed size / SHA-256 | 54,519,411 bytes / `73d280108fc9162ac51032c3f62bda871969d2bb826a8d2d253dd39a01231be3` |
| Persistent signer SHA-256 | `5db60116997ae5cf8ea0f26fee6c0a150a8045a7cbfc10282892ff10c0348004` |
| Alignment | `zipalign -c -P 16 4` passed |
| Signature | `apksigner verify --verbose --print-certs` passed with exactly one pinned signer |
| Payload | APK and nested assets contain no raw ROM, user OTR/O2R, save, state, or quest payload |
| Device actions | none; no ADB, installation, launch, or device control was performed |
| Publication | Public release `9.2.3.5`; normal, non-draft, non-prerelease release with exactly one APK and its matching checksum sidecar |
| Independent download | Downloaded through the GitHub release API into a separate `D:\Temp` directory; sidecar, package, numeric version, ABI, 16 KiB alignment, signer, signature, payload, and SHA-256 checks all passed |
