#ifndef DUAL_SCREEN_TRACKER_H
#define DUAL_SCREEN_TRACKER_H

#include <stdbool.h>

// Dual-screen trackers: renders the randomizer Item Tracker and Hint Tracker on
// the secondary (bottom) display instead of the gameplay HUD.

#ifdef __cplusplus
extern "C" {
#endif

// True when the bottom display should show the trackers (HUD then stays on the top screen).
bool DualScreenTracker_IsEnabled(void);

#ifdef __cplusplus
}

// Builds and renders one tracker frame. Must be called with the secondary EGL
// context current and the primary ImGui frame finished. Returns false when
// nothing could be drawn.
bool DualScreenTracker_Render(int surfaceWidth, int surfaceHeight);

// Loads the game textures used by the tracker screen. Must be called with the
// primary GL context current (before switching to the secondary surface).
void DualScreenTracker_PrepareResources();
#endif

#endif
