# Shipwright Android Dual Screen — Bleeding Edge

This is an **unofficial, source-only Android dual-screen port** of [HarbourMasters/Shipwright](https://github.com/HarbourMasters/Shipwright). It keeps the dual-screen implementation from [chilhaps/Shipwright-Android-Dual-Screen](https://github.com/chilhaps/Shipwright-Android-Dual-Screen), adapts it to the current renderer, and layers it over the newest validated **stable** Shipwright release with the modern Android compatibility work derived from [please-be-nice/Shipwright-android](https://github.com/please-be-nice/Shipwright-android).

“Bleeding Edge” means this repository is intended to follow the latest stable upstream Shipwright release. It does not use Shipwright development/nightly builds as its release base.

Current base: Shipwright `9.2.3` (`cb71e22a79bc5d1f688fa881795bbd93094895fc`). The exact source and submodule lineage is recorded in [Android provenance](docs/android/PROVENANCE.md), and the verified build contract is in [Android validation](docs/android/VALIDATION.md).

## Android dual-screen build

The Android app targets `arm64-v8a` with package `com.dishii.soh`. From the `Android` directory, run:

```powershell
.\gradlew.bat :app:testReleaseUnitTest :app:assembleRelease
```

The resulting APK is intentionally unsigned. Use your own persistent Android signing identity before installation or updates. This repository does not publish a private signing key, a locally signed APK, ROMs, generated game OTR/O2R files, saves, or other copyrighted game payloads.

Dual-screen routing is enabled by default on supported Android devices. If no suitable secondary presentation display exists, rendering safely remains on the primary display.

## Upstream project information

The remainder of this README comes from the official Shipwright project and describes its game requirements and general usage.

![Ship of Harkinian](docs/shiptitle.darkmode.png#gh-dark-mode-only)
![Ship of Harkinian](docs/shiptitle.lightmode.png#gh-light-mode-only)

* [Website](https://www.shipofharkinian.com)
* [Discord](https://discord.com/invite/harbourmasters)

If you're having any trouble after reading through this `README`, feel free to ask for help in the Support text channels. Please keep in mind that we do not condone piracy.

# Quick Start

The Ship does not include any copyrighted assets.  You are required to provide a supported copy of the game.

### 1. Verify your ROM dump
You can verify you have dumped a supported copy of the game by using the compatibility checker at https://ship.equipment/. If you'd prefer to manually validate your ROM dump, you can cross-reference its `sha1` hash with the hashes [here](docs/supportedHashes.json).

### 2. Download The Ship of Harkinian from [Releases](https://github.com/HarbourMasters/Shipwright/releases)

### 3. Launch the Game!
#### Windows
* Extract the zip
* Launch `soh.exe`

#### Linux
* Execute `soh.appimage`.  You may have to `chmod +x` the appimage via terminal.
* When prompted, select your supported copy of the game.
* Saves, settings and `oot.o2r` are stored in `~/.local/share/soh/`. If the folder you launch from already has `shipofharkinian.json`, `oot.o2r` or `oot-mq.o2r`, that folder is used instead. Set `SHIP_HOME` to choose another folder.

#### macOS
* Run `soh.app`. When prompted, select your supported copy of the game.
* You should see a notification saying `Processing OTR`, then, once the process is complete, you should get a notification saying `OTR Successfully Generated`, then the game should start.

### 4. Play!

Congratulations, you are now sailing with the Ship of Harkinian! Have fun!

# Configuration

### Default keyboard configuration
| N64 | A | B | Z | Start | Analog stick | C buttons | D-Pad |
| - | - | - | - | - | - | - | - |
| Keyboard | X | C | Z | Space | WASD | Arrow keys | TFGH |

### Other shortcuts
| Keys | Action |
| - | - |
| ESC | Toggle menu |
| F2 | Toggle capture mouse input |
| F5 | Save state |
| F6 | Change state |
| F7 | Load state |
| F9 | Toggle Text-to-Speech |
| F11 | Fullscreen |
| Tab | Toggle Alternate assets |
| Ctrl+R | Reset |

# Project Overview
Ship of Harkinian (SOH) is built atop a custom library dubbed libultraship (LUS). Back in the N64 days, there was an SDK distributed to developers named libultra; LUS is designed to mimic the functionality of libultra on modern hardware. In addition, we are dependent on the source code provided by the OOT decompilation project.

In order for the game to function, you will require a **legally acquired** ROM for Ocarina of Time. Click [here](https://ship.equipment/) to check the compatibility of your specific rom. Any copyrighted assets are extracted from the ROM and reformatted as a .o2r archive file which the code uses.

### Graphics Backends
Currently, there are three rendering APIs supported: DirectX11 (Windows), OpenGL (all platforms), and Metal (MacOS). You can change which API to use in the `Settings` menu of the menubar, which requires a restart.  If you're having an issue with crashing, you can change the API in the `shipofharkinian.json` file by finding the line `gfxbackend:""` and changing the value to `sdl` for OpenGL. DirectX 11 is the default on Windows.

# Custom Assets

Custom assets are packed in `.otr` archive files. To use custom assets, place them in the `mods` folder.

If you're interested in creating and/or packing your own custom asset `.otr` files, check out the following tools:
* [**retro - OTR generator**](https://github.com/HarbourMasters64/retro)
* [**fast64 - Blender plugin**](https://github.com/HarbourMasters/fast64)

# Development
### Building

If you want to manually compile SoH, please consult the [building instructions](docs/BUILDING.md).

### Playtesting
If you want to playtest a continuous integration build, you can find them at the links below. Keep in mind that these are for playtesting only, and you will likely encounter bugs and possibly crashes. 

* [Windows](https://nightly.link/HarbourMasters/Shipwright/workflows/generate-builds/develop/soh-windows.zip)
* [macOS](https://nightly.link/HarbourMasters/Shipwright/workflows/generate-builds/develop/soh-mac.zip)
* [Linux](https://nightly.link/HarbourMasters/Shipwright/workflows/generate-builds/develop/soh-linux.zip)

### Further Reading
More detailed documentation can be found in the 'docs' directory, including the aforementioned [building instructions](docs/BUILDING.md).

* [Credits](docs/CREDITS.md)
* [Custom Music](docs/CUSTOM_MUSIC.md)
* [Formatting](docs/FORMATTING.md)
* [Controller Mapping](docs/GAME_CONTROLLER_DB.md)
* [Modding](docs/MODDING.md)
* [Versioning](docs/VERSIONING.md)

<a href="https://github.com/Kenix3/libultraship/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/poweredbylus.darkmode.png">
    <img alt="Powered by libultraship" src="./docs/poweredbylus.lightmode.png">
  </picture>
</a>
