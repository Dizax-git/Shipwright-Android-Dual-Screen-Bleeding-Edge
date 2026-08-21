#ifndef DUAL_SCREEN_HUD_H
#define DUAL_SCREEN_HUD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void DualScreenHUD_InitFramebuffer(void);
bool DualScreenHUD_BeginHudPass(void);
void DualScreenHUD_EndHudPass(void);
void DualScreenHUD_PresentFrame(void);
bool DualScreenHUD_IsActive(void);

#ifdef __cplusplus
}
#endif

#endif
