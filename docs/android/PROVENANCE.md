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
- The Waterdish reference defines the original Android user-facing and build behavior. Its implementation is carried forward through the audited modern Android layer rather than by importing obsolete 9.0.2 gameplay code.
- The Chilhaps reference defines the dual-screen behavior and is adapted to the maintained renderer API. The Thor composition additionally moves the preserved Waterdish touch layer to the secondary presentation with primary-display fallback.
- The reference Docker build is deliberately excluded. The maintained build contract is `scripts/android/Build-WindowsAndroidRelease.ps1`, which generates `soh.o2r` and the unsigned arm64 APK natively on Windows and is shared by local builds and CI.
- `docs/android/FEATURE_PARITY.md` maps each maintained behavior to current source and automated evidence.
- ROMs, generated OTRs, saves, and user content are excluded from the repository and APK.
## Non-asset tree-difference classification

The inventory compares official `9.2.3` to `android-reference/android-port-923` while excluding extractor and `soh/assets` payload trees. `Required-android` entries are platform/build dependencies. `Accepted-android-feature` entries are narrowly reviewed mobile input or stability behavior. `Excluded-divergence` entries remain on the official 9.2.3 implementation. No upstream workflow, README, randomizer, network/gameplay divergence, or reference-local SDK configuration is imported.

| Path | Status | Classification |
|---|---:|---|
| `.github/workflows/android-release.yml` | `A` | `excluded-divergence` |
| `.github/workflows/apt-deps.txt` | `M` | `excluded-divergence` |
| `.github/workflows/generate-builds.yml` | `D` | `excluded-divergence` |
| `.github/workflows/pr-artifacts.yml` | `D` | `excluded-divergence` |
| `.github/workflows/test-builds-on-distros.yml` | `D` | `excluded-divergence` |
| `.gitignore` | `M` | `required-android` |
| `.gitmodules` | `M` | `required-android` |
| `Android/.gitignore` | `A` | `required-android` |
| `Android/app/build.gradle` | `A` | `required-android` |
| `Android/app/gradle/wrapper/gradle-wrapper.jar` | `A` | `required-android` |
| `Android/app/gradle/wrapper/gradle-wrapper.properties` | `A` | `required-android` |
| `Android/app/gradlew` | `A` | `required-android` |
| `Android/app/gradlew.bat` | `A` | `required-android` |
| `Android/app/proguard-rules.pro` | `A` | `required-android` |
| `Android/app/src/main/AndroidManifest.xml` | `A` | `required-android` |
| `Android/app/src/main/assets/gamecontrollerdb.txt` | `A` | `required-android` |
| `Android/app/src/main/java/com/dishii/soh/AssetCopyUtil.java` | `A` | `required-android` |
| `Android/app/src/main/java/com/dishii/soh/ControllerButtons.java` | `A` | `required-android` |
| `Android/app/src/main/java/com/dishii/soh/MainActivity.java` | `A` | `required-android` |
| `Android/app/src/main/java/org/libsdl/app/HIDDevice.java` | `A` | `required-android` |
| `Android/app/src/main/java/org/libsdl/app/HIDDeviceBLESteamController.java` | `A` | `required-android` |
| `Android/app/src/main/java/org/libsdl/app/HIDDeviceManager.java` | `A` | `required-android` |
| `Android/app/src/main/java/org/libsdl/app/HIDDeviceUSB.java` | `A` | `required-android` |
| `Android/app/src/main/java/org/libsdl/app/SDL.java` | `A` | `required-android` |
| `Android/app/src/main/java/org/libsdl/app/SDLActivity.java` | `A` | `required-android` |
| `Android/app/src/main/java/org/libsdl/app/SDLAudioManager.java` | `A` | `required-android` |
| `Android/app/src/main/java/org/libsdl/app/SDLControllerManager.java` | `A` | `required-android` |
| `Android/app/src/main/java/org/libsdl/app/SDLSurface.java` | `A` | `required-android` |
| `Android/app/src/main/res/drawable/ic_button.xml` | `A` | `required-android` |
| `Android/app/src/main/res/drawable/ic_cross.xml` | `A` | `required-android` |
| `Android/app/src/main/res/drawable/ic_launcher_background.xml` | `A` | `required-android` |
| `Android/app/src/main/res/drawable/ic_launcher_foreground.xml` | `A` | `required-android` |
| `Android/app/src/main/res/drawable/ic_rectangular_button.xml` | `A` | `required-android` |
| `Android/app/src/main/res/drawable/ic_show.xml` | `A` | `required-android` |
| `Android/app/src/main/res/drawable/ic_stick.xml` | `A` | `required-android` |
| `Android/app/src/main/res/drawable/ic_trigger_button_left.xml` | `A` | `required-android` |
| `Android/app/src/main/res/drawable/ic_trigger_button_right.xml` | `A` | `required-android` |
| `Android/app/src/main/res/layout/touchcontrol_overlay.xml` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-anydpi-v26/ic_launcher_round.xml` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-hdpi/ic_launcher.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-hdpi/ic_launcher_foreground.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-hdpi/ic_launcher_round.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-mdpi/ic_launcher.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-mdpi/ic_launcher_foreground.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-mdpi/ic_launcher_round.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-xhdpi/ic_launcher.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-xhdpi/ic_launcher_foreground.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-xhdpi/ic_launcher_round.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-xxhdpi/ic_launcher.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-xxhdpi/ic_launcher_foreground.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-xxhdpi/ic_launcher_round.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-xxxhdpi/ic_launcher.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-xxxhdpi/ic_launcher_foreground.webp` | `A` | `required-android` |
| `Android/app/src/main/res/mipmap-xxxhdpi/ic_launcher_round.webp` | `A` | `required-android` |
| `Android/app/src/main/res/values/colors.xml` | `A` | `required-android` |
| `Android/app/src/main/res/values/ic_launcher_background.xml` | `A` | `required-android` |
| `Android/app/src/main/res/values/strings.xml` | `A` | `required-android` |
| `Android/app/src/main/res/values/styles.xml` | `A` | `required-android` |
| `Android/build.gradle` | `A` | `required-android` |
| `Android/gradle.properties` | `A` | `required-android` |
| `Android/gradle/wrapper/gradle-wrapper.jar` | `A` | `required-android` |
| `Android/gradle/wrapper/gradle-wrapper.properties` | `A` | `required-android` |
| `Android/gradlew` | `A` | `required-android` |
| `Android/gradlew.bat` | `A` | `required-android` |
| `Android/local.properties` | `A` | `excluded-divergence` |
| `Android/settings.gradle` | `A` | `required-android` |
| `CMakeLists.txt` | `M` | `required-android` |
| `OTRExporter` | `M` | `required-android` |
| `README.md` | `M` | `excluded-divergence` |
| `ZAPDTR` | `M` | `required-android` |
| `docker/Containerfile` | `A` | `excluded-divergence` |
| `docker/Makefile` | `A` | `excluded-divergence` |
| `docker/scripts/build_android_project.sh` | `A` | `excluded-divergence` |
| `docker/scripts/build_container_image.sh` | `A` | `excluded-divergence` |
| `docker/scripts/entrypoint.sh` | `A` | `excluded-divergence` |
| `docker/scripts/install_container_image.sh` | `A` | `excluded-divergence` |
| `docker/scripts/publish_container_image.sh` | `A` | `excluded-divergence` |
| `docker/scripts/set_container_vars.sh` | `A` | `excluded-divergence` |
| `libultraship` | `M` | `required-android` |
| `soh/CMakeLists.txt` | `M` | `required-android` |
| `soh/fixbaserom.py` | `M` | `required-android` |
| `soh/include/functions.h` | `M` | `required-android` |
| `soh/soh/Enhancements/BlueFireArrows.cpp` | `A` | `excluded-divergence` |
| `soh/soh/Enhancements/Lang/Lang.cpp` | `A` | `required-android` |
| `soh/soh/Enhancements/Lang/Lang.h` | `A` | `required-android` |
| `soh/soh/Enhancements/bootcommands.c` | `M` | `accepted-android-feature` |
| `soh/soh/Enhancements/controls/SohInputEditorWindow.cpp` | `M` | `accepted-android-feature` |
| `soh/soh/Enhancements/customequipment.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/debugger/SohConsoleWindow.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/debugger/SohConsoleWindow.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/debugger/debugSaveEditor.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/debugger/valueViewer.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/debugger/valueViewer.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/game-interactor/GameInteractor_HookTable.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/game-interactor/GameInteractor_Hooks.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/game-interactor/vanilla-behavior/GIVanillaBehavior.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/mod_menu.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/3drando/hint_list.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/3drando/hint_list/hint_list_exclude_dungeon.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/3drando/hint_list/hint_list_exclude_overworld.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/SeedContext.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/ShuffleFairies.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/ShuffleSigns.cpp` | `A` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/dungeons/bottom_of_the_well.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/dungeons/dodongos_cavern.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/dungeons/jabujabus_belly.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/dungeons/shadow_temple.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/dungeons/spirit_temple.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/castle_grounds.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/death_mountain_crater.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/death_mountain_trail.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/gerudo_fortress.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/gerudo_valley.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/goron_city.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/graveyard.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/haunted_wasteland.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/hyrule_field.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/kakariko.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/kokiri_forest.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/lake_hylia.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/lost_woods.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/market.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/temple_of_time.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/zoras_domain.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/zoras_fountain.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/location_access/overworld/zoras_river.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/logic.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/logic.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/option.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/option.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/option_descriptions.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizer.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizer.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizerEnums/RandomizerCheck.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizerEnums/RandomizerHintTextKey.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizerEnums/RandomizerInf.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizerEnums/RandomizerMiscEnums.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizerEnums/RandomizerOptions.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizerEnums/RandomizerSettingKey.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizerEnums/RandomizerTrick.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizer_check_objects.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/randomizer_check_tracker.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/settings.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/settings.h` | `M` | `excluded-divergence` |
| `soh/soh/Enhancements/randomizer/static_data.h` | `M` | `excluded-divergence` |
| `soh/soh/Extractor/Extract.cpp` | `M` | `required-android` |
| `soh/soh/Network/Anchor/Anchor.h` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/HookHandlers.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/JsonConversions.hpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Menu.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/DamagePlayer.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/EntranceDiscovered.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/GameComplete.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/GiveItem.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/OcarinaSfx.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/PlayerSfx.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/RequestTeleport.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/ServerMessage.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/SetCheckStatus.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/SetFlag.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/TeleportTo.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/UnsetFlag.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/UpdateBeansCount.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/UpdateClientState.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/UpdateDungeonItems.cpp` | `M` | `excluded-divergence` |
| `soh/soh/Network/Anchor/Packets/UpdateTeamState.cpp` | `M` | `excluded-divergence` |
| `soh/soh/OTRGlobals.cpp` | `M` | `accepted-android-feature` |
| `soh/soh/SohGui/Menu.cpp` | `M` | `excluded-divergence` |
| `soh/soh/SohGui/SohGui.cpp` | `M` | `excluded-divergence` |
| `soh/soh/SohGui/SohMenuNetwork.cpp` | `M` | `accepted-android-feature` |
| `soh/soh/SohGui/SohMenuRandomizer.cpp` | `M` | `required-android` |
| `soh/soh/SohGui/SohModals.cpp` | `M` | `required-android` |
| `soh/soh/SohGui/UIWidgets.cpp` | `M` | `excluded-divergence` |
| `soh/soh/SohGui/UIWidgets.hpp` | `M` | `excluded-divergence` |
| `soh/soh/util.cpp` | `M` | `excluded-divergence` |
| `soh/soh/util.h` | `M` | `excluded-divergence` |
| `soh/src/code/audio_load.c` | `M` | `required-android` |
| `soh/src/code/main.c` | `M` | `required-android` |
| `soh/src/code/z_bgcheck.c` | `M` | `accepted-android-feature` |
| `soh/src/code/z_camera.c` | `M` | `accepted-android-feature` |
| `soh/src/code/z_map_exp.c` | `M` | `excluded-divergence` |
| `soh/src/overlays/actors/ovl_Bg_Breakwall/z_bg_breakwall.c` | `M` | `excluded-divergence` |
| `soh/src/overlays/actors/ovl_Bg_Ice_Shelter/z_bg_ice_shelter.c` | `M` | `excluded-divergence` |
| `soh/src/overlays/actors/ovl_Bg_Ice_Shelter/z_bg_ice_shelter.h` | `M` | `excluded-divergence` |
| `soh/src/overlays/actors/ovl_En_Butte/z_en_butte.c` | `M` | `excluded-divergence` |
| `soh/src/overlays/actors/ovl_player_actor/z_player.c` | `M` | `accepted-android-feature` |
