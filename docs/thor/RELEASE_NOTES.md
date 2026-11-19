First launch fixes.

- The first launch copies its files much faster (under 2 minutes on my Thor instead of several minutes on a black screen), with a counter showing the progress. If it gets interrupted, it picks up again at the next launch.
- If your front-end (Cocoon or another) starts the game on the bottom screen, it moves itself to the top screen.
- On-screen touch controls and their eye button are hidden on a new install. If you never touched the eye in v1.0.0, they get hidden too. To bring them back: Settings > Controls, "Link (P1)" tab > Touch Controls, untick "Hide Overlay Toggle Button", then tap the eye.

**Updating from v1.0.0:** install the APK over the old one. Saves, settings and `oot.o2r` are kept, nothing is copied again.

**New install:** uninstall any other SoH build that uses `com.dishii.soh` (Darkaxt's port for example), this APK is signed with a different key. Your `SOH` folder stays, but back it up. When you allow file access, press Back twice on the Thor to get back to the game. On the Thor, set Autolock to the top screen in the AYN settings.

Tested on my Thor only (fresh install, interrupted copy, launch from the bottom screen, quit from Cocoon). Everything else is in the [README](https://github.com/Dizax-git/Shipwright-Android-Dual-Screen-Bleeding-Edge#readme). The `.sha256` file is the checksum of the APK.
