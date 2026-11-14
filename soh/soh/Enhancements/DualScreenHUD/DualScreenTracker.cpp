#include "DualScreenTracker.h"

#include "libultraship/bridge/consolevariablebridge.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/gameplaystats.h"
#include "soh/ShipUtils.h"

#include <imgui.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
#include <z64.h>
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
    float scale = CVarGetFloat(CVAR_ENHANCEMENT("DualScreenTrackerScale"), 1.6f);
    return std::clamp(scale, 1.0f, 3.0f);
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

    const float fontSize = io.Fonts->Fonts.empty() ? 13.0f : io.Fonts->Fonts[0]->FontSize;
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
    style.ScrollbarSize = 10.0f;

    ImGui::SetCurrentContext(primary);
    sLastFrame = std::chrono::steady_clock::now();
    return true;
}

void DrawTabButton(const char* label, TrackerTab tab, float width, float height) {
    const bool selected = CVarGetInteger(CVAR_ENHANCEMENT("DualScreenTrackerTab"), TAB_ITEMS) == tab;
    const ImVec4 accent = ImGui::GetStyle().Colors[ImGuiCol_TabSelected];
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? accent : ImVec4(0.16f, 0.16f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? accent : ImVec4(0.22f, 0.22f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
    if (ImGui::Button(label, ImVec2(width, height)) && !selected) {
        CVarSetInteger(CVAR_ENHANCEMENT("DualScreenTrackerTab"), tab);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    ImGui::PopStyleColor(3);
}

void DrawTrackerUi() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##DualScreenTrackers", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Status line: run timer (same as the Gameplay Stats total time) and in-game clock.
    if (GameInteractor::IsSaveLoaded()) {
        const std::string runTime = Ship_FormatTimeDisplay(static_cast<uint32_t>(GAMEPLAYSTAT_TOTAL_TIME));
        const uint32_t daySeconds =
            static_cast<uint32_t>(static_cast<double>(gSaveContext.dayTime) * (24 * 60 * 60 - 1) / 65535);
        const bool isNight = gSaveContext.nightFlag != 0;
        ImGui::Text("%s %s", Label("Run", "Lauf", "Run"), runTime.c_str());
        ImGui::SameLine();
        const std::string clock = std::string(Label("Time", "Zeit", "Heure")) + " " +
                                  std::to_string(daySeconds / 3600 / 10) + std::to_string(daySeconds / 3600 % 10) +
                                  ":" + std::to_string(daySeconds % 3600 / 60 / 10) +
                                  std::to_string(daySeconds % 3600 / 60 % 10) + " (" +
                                  (isNight ? Label("night", "Nacht", "nuit") : Label("day", "Tag", "jour")) + ")";
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(clock.c_str()).x);
        ImGui::TextColored(isNight ? ImVec4(0.6f, 0.7f, 1.0f, 1.0f) : ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s",
                           clock.c_str());
    }

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float tabWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
    const float tabHeight = 34.0f;
    DrawTabButton(Label("Items", "Gegenstaende", "Objets"), TAB_ITEMS, tabWidth, tabHeight);
    ImGui::SameLine();
    DrawTabButton(Label("Hints", "Hinweise", "Indices"), TAB_HINTS, tabWidth, tabHeight);

    const int tab = CVarGetInteger(CVAR_ENHANCEMENT("DualScreenTrackerTab"), TAB_ITEMS);
    if (tab == TAB_HINTS) {
        ImGui::BeginChild("##DualScreenHints", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
        HintTracker::DrawEmbedded();
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

#endif
