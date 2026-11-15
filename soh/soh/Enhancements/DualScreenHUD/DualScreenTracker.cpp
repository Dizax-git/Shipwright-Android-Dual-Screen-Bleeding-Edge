#include "DualScreenTracker.h"

#include "libultraship/bridge/consolevariablebridge.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/gameplaystats.h"
#include "soh/ShipUtils.h"
#include "soh/OTRGlobals.h"
#include <fast/Fast3dGui.h>
#include <ship/window/gui/IconsFontAwesome4.h>

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
#include <z64.h>
#include "assets/textures/icon_item_static/icon_item_static.h"
#include "assets/textures/message_static/message_static.h"
#include "assets/textures/parameter_static/parameter_static.h"
#include "variables.h"
#include "macros.h"
}

// Defined in randomizer_item_tracker.cpp / randomizer_hint_tracker.cpp.
void ItemTracker_DrawEmbedded();
namespace HintTracker {
void DrawEmbedded();
}

bool DualScreenTracker_IsEnabled(void) {
#if defined(__ANDROID__)
    return CVarGetInteger(CVAR_ENHANCEMENT("DualScreenHUD"), 1) != 0 &&
           CVarGetInteger(CVAR_ENHANCEMENT("DualScreenBottom"), 1) == 1;
#else
    return false;
#endif
}

#if defined(__ANDROID__)

#include <GLES3/gl3.h>
#include <android/log.h>
#include <imgui_impl_opengl3.h>
#include <jni.h>

namespace {

constexpr const char* kLogTag = "SoH-DualScreenTracker";

// Android MotionEvent action codes (getActionMasked()).
constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;

// A finger that moves more than this (in logical pixels) scrolls instead of clicking.
constexpr float kScrollThreshold = 10.0f;

enum TrackerTab {
    TAB_ITEMS = 0,
    TAB_HINTS = 1,
};

struct TouchEvent {
    int action;
    float x;
    float y;
};

std::mutex sTouchMutex;
std::vector<TouchEvent> sTouchQueue;

ImGuiContext* sContext = nullptr;
bool sBackendReady = false;
bool sBackendFailed = false;
std::chrono::steady_clock::time_point sLastFrame;

// Touch gesture state (logical coordinates).
bool sFingerDown = false;
bool sScrolling = false;
float sDownX = 0.0f;
float sDownY = 0.0f;
float sLastY = 0.0f;
bool sReleaseHover = false;

float GetUiScale() {
    float scale = CVarGetFloat(CVAR_ENHANCEMENT("DualScreenTrackerScale"), 2.0f);
    return std::clamp(scale, 1.5f, 3.0f);
}

// Large fonts (rasterized at screen resolution, drawn at logical size: sharp text).
ImFont* BigFont() {
    return OTRGlobals::Instance->fontDualScreen != nullptr ? OTRGlobals::Instance->fontDualScreen
                                                           : OTRGlobals::Instance->fontStandardLarger;
}
ImFont* HugeFont() {
    return OTRGlobals::Instance->fontDualScreenLarge != nullptr ? OTRGlobals::Instance->fontDualScreenLarge
                                                                : OTRGlobals::Instance->fontStandardLargest;
}

const char* Label(const char* en, const char* de, const char* fr) {
    switch (CVarGetInteger(CVAR_SETTING("Languages"), 0)) {
        case 1:
            return de;
        case 2:
            return fr;
        default:
            return en;
    }
}

void ApplyTouchEvents(float scale) {
    std::vector<TouchEvent> events;
    {
        std::lock_guard<std::mutex> lock(sTouchMutex);
        events.swap(sTouchQueue);
    }

    ImGuiIO& io = ImGui::GetIO();
    if (sReleaseHover && !sFingerDown) {
        // Move the virtual mouse away after a tap so nothing stays hovered.
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        sReleaseHover = false;
    }

    const float fontSize = BigFont()->FontSize / scale;
    for (const TouchEvent& event : events) {
        const float x = event.x / scale;
        const float y = event.y / scale;
        switch (event.action) {
            case kActionDown:
                sFingerDown = true;
                sScrolling = false;
                sDownX = x;
                sDownY = y;
                sLastY = y;
                io.AddMousePosEvent(x, y);
                break;
            case kActionMove:
                if (!sFingerDown) {
                    break;
                }
                if (!sScrolling && (std::fabs(y - sDownY) > kScrollThreshold ||
                                    std::fabs(x - sDownX) > kScrollThreshold)) {
                    sScrolling = true;
                }
                if (sScrolling) {
                    // ImGui scrolls 5 lines per wheel unit.
                    const float dy = y - sLastY;
                    sLastY = y;
                    if (dy != 0.0f) {
                        io.AddMouseWheelEvent(0.0f, dy / (5.0f * fontSize));
                    }
                }
                break;
            case kActionUp:
                if (sFingerDown && !sScrolling) {
                    io.AddMousePosEvent(x, y);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
                }
                sFingerDown = false;
                sScrolling = false;
                sReleaseHover = true;
                break;
            case kActionCancel:
                sFingerDown = false;
                sScrolling = false;
                sReleaseHover = true;
                break;
            default:
                break;
        }
    }
}

bool EnsureContext() {
    if (sContext != nullptr) {
        return true;
    }

    ImGuiContext* primary = ImGui::GetCurrentContext();
    if (primary == nullptr) {
        return false;
    }
    const ImGuiStyle primaryStyle = ImGui::GetStyle();

    // Share the primary font atlas so the tracker fonts (mono, icons) are available.
    sContext = ImGui::CreateContext(ImGui::GetIO().Fonts);
    ImGui::SetCurrentContext(sContext);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
    io.MouseDrawCursor = false;

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    std::memcpy(style.Colors, primaryStyle.Colors, sizeof(style.Colors));
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8.0f, 6.0f);
    style.FrameRounding = 6.0f;
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 6.0f;
    style.Colors[ImGuiCol_Header] = ImVec4(0.16f, 0.14f, 0.24f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.19f, 0.32f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.22f, 0.38f, 1.0f);

    ImGui::SetCurrentContext(primary);
    sLastFrame = std::chrono::steady_clock::now();
    return true;
}

bool sTexturesLoaded = false;

ImTextureID Tex(const char* name) {
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    return gui == nullptr ? nullptr : gui->GetTextureByName(name);
}

// Game-style text box (the dialogue box texture, mirrored for its right half).
void DrawTextbox(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 color) {
    ImTextureID box = Tex("DST_MSG_BG");
    if (box == nullptr) {
        drawList->AddRectFilled(min, max, color, 8.0f);
        return;
    }
    const float midX = (min.x + max.x) * 0.5f;
    drawList->AddImage(box, min, ImVec2(midX, max.y), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), color);
    drawList->AddImage(box, ImVec2(midX, min.y), max, ImVec2(1.0f, 0.0f), ImVec2(0.0f, 1.0f), color);
}

// The pause menu selection cursor: four pulsing corners around the selection.
void DrawPauseCursor(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 5.0f);
    const ImU32 color = IM_COL32(255, 255, static_cast<int>(60 + 150 * pulse), 255);
    const float size = 16.0f;
    const float inset = 4.0f;
    struct Corner {
        const char* name;
        ImVec2 pos;
    };
    const Corner corners[] = {
        { "DST_CURSOR_TL", ImVec2(min.x - inset, min.y - inset) },
        { "DST_CURSOR_TR", ImVec2(max.x + inset - size, min.y - inset) },
        { "DST_CURSOR_BL", ImVec2(min.x - inset, max.y + inset - size) },
        { "DST_CURSOR_BR", ImVec2(max.x + inset - size, max.y + inset - size) },
    };
    for (const Corner& corner : corners) {
        ImTextureID tex = Tex(corner.name);
        if (tex != nullptr) {
            drawList->AddImage(tex, corner.pos, ImVec2(corner.pos.x + size, corner.pos.y + size), ImVec2(0, 0),
                               ImVec2(1, 1), color);
        }
    }
}

void DrawTabButton(const char* label, TrackerTab tab, float width, float height) {
    const bool selected = CVarGetInteger(CVAR_ENHANCEMENT("DualScreenTrackerTab"), TAB_ITEMS) == tab;
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + width, min.y + height);
    ImGui::PushID(tab);
    if (ImGui::InvisibleButton("##tab", ImVec2(width, height)) && !selected) {
        CVarSetInteger(CVAR_ENHANCEMENT("DualScreenTrackerTab"), tab);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    ImGui::PopID();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawTextbox(drawList, min, max, selected ? IM_COL32(20, 20, 60, 235) : IM_COL32(0, 0, 0, 190));
    ImFont* font = HugeFont();
    const float fontSize = 23.0f;
    const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
    const ImVec2 textPos(min.x + (width - textSize.x) * 0.5f, min.y + (height - textSize.y) * 0.5f);
    drawList->AddText(font, fontSize, ImVec2(textPos.x + 2.0f, textPos.y + 2.0f), IM_COL32(0, 0, 0, 200), label);
    drawList->AddText(font, fontSize, textPos, selected ? IM_COL32(255, 255, 120, 255) : IM_COL32(200, 200, 200, 255),
                      label);
    if (selected) {
        DrawPauseCursor(drawList, min, max);
    }
}

void DrawTrackerUi() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##DualScreenTrackers", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImDrawList* bg = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        bg->AddRectFilledMultiColor(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(18, 20, 44, 255),
                                    IM_COL32(18, 20, 44, 255), IM_COL32(4, 4, 12, 255), IM_COL32(4, 4, 12, 255));
    }

    // Status line: run timer (same as the Gameplay Stats total time) and in-game clock.
    if (GameInteractor::IsSaveLoaded()) {
        const std::string runTime = Ship_FormatTimeDisplay(static_cast<uint32_t>(GAMEPLAYSTAT_TOTAL_TIME));
        const uint32_t daySeconds =
            static_cast<uint32_t>(static_cast<double>(gSaveContext.dayTime) * (24 * 60 * 60 - 1) / 65535);
        const bool isNight = gSaveContext.nightFlag != 0;
        char clock[16];
        std::snprintf(clock, sizeof(clock), "%02u:%02u", daySeconds / 3600, daySeconds % 3600 / 60);

        // Large status bar: readable at a glance on the small bottom screen.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImFont* font = HugeFont();
        const float textSize = 25.0f;
        const ImVec2 barMin = ImGui::GetCursorScreenPos();
        const float barHeight = textSize + 22.0f;
        const ImVec2 barMax(barMin.x + ImGui::GetContentRegionAvail().x, barMin.y + barHeight);
        DrawTextbox(drawList, barMin, barMax, IM_COL32(0, 0, 0, 200));

        const float textY = barMin.y + (barHeight - textSize) * 0.5f;
        auto shadowText = [&](ImVec2 pos, ImU32 color, const char* text) {
            drawList->AddText(font, textSize, ImVec2(pos.x + 2.0f, pos.y + 2.0f), IM_COL32(0, 0, 0, 220), text);
            drawList->AddText(font, textSize, pos, color, text);
        };
        const std::string runLabel = std::string(ICON_FA_HOURGLASS_HALF " ") + runTime;
        shadowText(ImVec2(barMin.x + 20.0f, textY), IM_COL32(255, 255, 255, 255), runLabel.c_str());

        const ImU32 clockColor = isNight ? IM_COL32(150, 180, 255, 255) : IM_COL32(255, 220, 100, 255);
        const float iconSize = textSize;
        const float clockWidth = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, clock).x;
        const float clockX = barMax.x - 20.0f - clockWidth;
        ImTextureID clockIcon = Tex("DST_CLOCK");
        if (clockIcon != nullptr) {
            drawList->AddImage(clockIcon, ImVec2(clockX - iconSize - 10.0f, textY),
                               ImVec2(clockX - 10.0f, textY + iconSize), ImVec2(0, 0), ImVec2(1, 1), clockColor);
        }
        shadowText(ImVec2(clockX, textY), clockColor, clock);
        ImGui::Dummy(ImVec2(0.0f, barHeight + 8.0f));
    }

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float tabWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
    const float tabHeight = 50.0f;
    DrawTabButton(Label("Items", "Gegenstaende", "Objets"), TAB_ITEMS, tabWidth, tabHeight);
    ImGui::SameLine();
    DrawTabButton(Label("Hints", "Hinweise", "Indices"), TAB_HINTS, tabWidth, tabHeight);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const int tab = CVarGetInteger(CVAR_ENHANCEMENT("DualScreenTrackerTab"), TAB_ITEMS);
    if (tab == TAB_HINTS) {
        ImGui::BeginChild("##DualScreenHints", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
        ImGui::PushFont(BigFont());
        HintTracker::DrawEmbedded();
        ImGui::PopFont();
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("##DualScreenItems", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
        ItemTracker_DrawEmbedded();
        ImGui::EndChild();
    }

    ImGui::End();
}

} // namespace

bool DualScreenTracker_Render(int surfaceWidth, int surfaceHeight) {
    if (sBackendFailed || surfaceWidth <= 0 || surfaceHeight <= 0) {
        return false;
    }

    ImGuiContext* primary = ImGui::GetCurrentContext();
    if (primary == nullptr || !EnsureContext()) {
        return false;
    }

    // The atlas is shared: keep the primary font texture id intact even if the
    // secondary renderer backend uploads its own copy of the font texture.
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    ImTextureID primaryFontTexture = atlas->TexID;

    ImGui::SetCurrentContext(sContext);
    if (!sBackendReady) {
        sBackendReady = ImGui_ImplOpenGL3_Init("#version 300 es");
        if (!sBackendReady) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag, "ImGui OpenGL3 backend init failed");
            sBackendFailed = true;
            atlas->SetTexID(primaryFontTexture);
            ImGui::SetCurrentContext(primary);
            return false;
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    const float scale = GetUiScale();
    io.DisplaySize = ImVec2(surfaceWidth / scale, surfaceHeight / scale);
    io.DisplayFramebufferScale = ImVec2(scale, scale);
    // Fonts are rasterized at screen size: draw them at 1/scale so they land 1:1 on the panel.
    io.FontGlobalScale = 1.0f / scale;
    io.FontDefault = BigFont();

    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - sLastFrame).count();
    sLastFrame = now;
    io.DeltaTime = std::clamp(deltaTime, 1.0f / 240.0f, 0.25f);

    ApplyTouchEvents(scale);

    ImGui_ImplOpenGL3_NewFrame();
    atlas->SetTexID(primaryFontTexture);
    ImGui::NewFrame();
    DrawTrackerUi();
    ImGui::Render();

    glViewport(0, 0, surfaceWidth, surfaceHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    atlas->SetTexID(primaryFontTexture);
    ImGui::SetCurrentContext(primary);
    return true;
}

void DualScreenTracker_PrepareResources() {
    if (sTexturesLoaded) {
        return;
    }
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if (gui == nullptr) {
        return;
    }
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    gui->LoadGuiTexture("DST_MSG_BG", dgDefaultMessageBackgroundTex, "", white);
    gui->LoadGuiTexture("DST_CURSOR_TL", dgPauseMenuCursorTopLeftTex, "", white);
    gui->LoadGuiTexture("DST_CURSOR_TR", dgPauseMenuCursorTopRightTex, "", white);
    gui->LoadGuiTexture("DST_CURSOR_BL", dgPauseMenuCursorBottomLeftTex, "", white);
    gui->LoadGuiTexture("DST_CURSOR_BR", dgPauseMenuCursorBottomRightTex, "", white);
    gui->LoadGuiTexture("DST_CLOCK", dgClockIconTex, "", white);
    sTexturesLoaded = true;
}

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_MainActivity_nativeSecondaryTouch(JNIEnv*, jobject,
                                                                                       jint action, jfloat x,
                                                                                       jfloat y) {
    std::lock_guard<std::mutex> lock(sTouchMutex);
    if (sTouchQueue.size() < 256) {
        sTouchQueue.push_back({ static_cast<int>(action), static_cast<float>(x), static_cast<float>(y) });
    }
}

#else

bool DualScreenTracker_Render(int, int) {
    return false;
}

void DualScreenTracker_PrepareResources() {
}

#endif
