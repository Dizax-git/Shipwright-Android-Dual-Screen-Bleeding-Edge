# Android Dual-Screen Source Provenance

Verified on 2026-08-21. Official stable source is the newest non-draft, non-prerelease HarbourMasters release available at implementation start.

## Source refs

| Repository | Ref | Commit |
|---|---|---|
| HarbourMasters/Shipwright | `9.2.3` | cb71e22a79bc5d1f688fa881795bbd93094895fc |
| please-be-nice/Shipwright-android | `android-reference/android-port-923` | f8dc697418096324ead7420dafbceabfcc3bf8bc |
| chilhaps/Shipwright-Android-Dual-Screen | `dual-screen-reference/main` | 33185ad59e011959aa6a6fbcf4a854f123b7e4a3 |
| Waterdish/Shipwright-Android | `waterdish-reference/main` | c9d8f4a1219d4db0b577a050634679f72f7b3595 |
| Android libultraship gitlink | `android-reference/android-port-923:libultraship` | 3a9af4fc67ddc2d476f204fdc43f87dc363ee0e0 |
| Android OTRExporter gitlink | `android-reference/android-port-923:OTRExporter` | 77d47bc6d1767edb56f7cd461c5e1b7681f36a93 |
| Android ZAPDTR gitlink | `android-reference/android-port-923:ZAPDTR` | 1aa5bfc16cba8e487077a2a1f5ab7d5263ecf508 |

## Source policy

- Official game behavior comes from Shipwright 9.2.3.
- The modern Android reference supplies audited Android platform integration and Android-compatible submodules.
- The Chilhaps reference supplies only the dual-screen feature behavior and is adapted to the modern renderer API.
- The Waterdish reference is comparison evidence for the older 9.0.2 Android base and is not an implementation source.
- ROMs, generated OTRs, saves, and user content are excluded from the repository and APK.
