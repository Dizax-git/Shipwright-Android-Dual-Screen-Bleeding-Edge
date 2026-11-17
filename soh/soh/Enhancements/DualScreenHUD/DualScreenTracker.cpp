#include "DualScreenTracker.h"

#include "libultraship/bridge/consolevariablebridge.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/gameplaystats.h"
#include "soh/ShipUtils.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/location.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/hint.h"
#include "soh/Enhancements/randomizer/randomizer_hint_tracker.h"
#include "soh/ResourceManagerHelpers.h"
#include <fast/Fast3dGui.h>
#include <fast/resource/type/Texture.h>
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
#include <unordered_map>
#include <vector>

extern "C" {
#include <z64.h>
#include "assets/textures/icon_item_static/icon_item_static.h"
#include "assets/textures/message_static/message_static.h"
#include "assets/textures/parameter_static/parameter_static.h"
#include "assets/textures/icon_item_field_static/icon_item_field_static.h"
#include "assets/soh_assets.h"
#include "variables.h"
#include "macros.h"
}

// Game font: glyph textures and widths (z_kanfont.c, z_message_PAL.c).
extern "C" const char* fontTbl[140];
extern "C" f32 sFontWidths[144];

// Defined in randomizer_item_tracker.cpp.
void ItemTracker_DrawEmbedded();

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
#include <imgui_internal.h>
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
    TAB_CHECKS = 2,
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

// ---- Game font (the dialogue font of the game, nes_font_static) ----
// The bottom screen writes with the game's own letters: the HD texture pack
// version when there is one (alt/), otherwise the original 16x16 glyphs.
// Each pixel size gets one atlas of the 140 glyphs, built at the exact size it
// is drawn at (the GUI samples at the nearest texel). Atlases are built in
// DualScreenTracker_PrepareResources (main GL context); until one is ready,
// the text falls back to the interface font for that frame.
namespace GameFont {

constexpr int kGlyphCount = 140;
constexpr int kAtlasCols = 16;
constexpr int kAtlasRows = 9;
// Caps sit between 7 % and 77 % of the 16x16 cell: the cap line is matched with
// the interface font so text keeps its place, then scaled by role.
constexpr float kCapTop = 0.07f;
constexpr float kCapHeight = 0.70f;
constexpr float kSpacing = 0.88f; // a bit tighter than the dialogue boxes
constexpr float kLigature = 0.72f; // "oe": the o overlaps the e

enum class AtlasState { Requested, Ready, Failed };
std::unordered_map<int, AtlasState> sAtlases;
int sAvailable = -1; // can the glyphs be loaded at all?

bool Enabled() {
    return CVarGetInteger(CVAR_ENHANCEMENT("DualScreenGameFont"), 1) != 0 && sAvailable != 0;
}

// Text role: titles bigger than lists, lists a little bigger than details.
float RoleScale(float size) {
    if (size >= 20.0f) {
        return 1.2f;
    }
    if (size >= 17.0f) {
        return 1.05f;
    }
    return 1.0f;
}

// UTF-8 -> game character (0x20-0xAB), 0x100 + code for the "oe" ligature
// halves, or -1 when the game font has no such letter.
int Decode(const char*& p, const char* end, unsigned int& codepoint) {
    const unsigned char c = static_cast<unsigned char>(*p);
    int len = 1;
    codepoint = c;
    if (c >= 0xF0 && p + 3 < end) {
        codepoint = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        len = 4;
    } else if (c >= 0xE0 && p + 2 < end) {
        codepoint = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        len = 3;
    } else if (c >= 0xC0 && p + 1 < end) {
        codepoint = ((c & 0x1F) << 6) | (p[1] & 0x3F);
        len = 2;
    }
    p += len;
    if (codepoint >= 0x20 && codepoint < 0x7F) {
        return static_cast<int>(codepoint);
    }
    switch (codepoint) {
        case 0x00C0: return 0x80; // À
        case 0x00EE: return 0x81; // î
        case 0x00C2: return 0x82; // Â
        case 0x00C4: return 0x83; // Ä
        case 0x00C7: return 0x84; // Ç
        case 0x00C8: return 0x85; // È
        case 0x00C9: return 0x86; // É
        case 0x00CA: return 0x87; // Ê
        case 0x00CB: return 0x88; // Ë
        case 0x00CF: return 0x89; // Ï
        case 0x00D4: return 0x8A; // Ô
        case 0x00D6: return 0x8B; // Ö
        case 0x00D9: return 0x8C; // Ù
        case 0x00DB: return 0x8D; // Û
        case 0x00DC: return 0x8E; // Ü
        case 0x00DF: return 0x8F; // ß
        case 0x00E0: return 0x90; // à
        case 0x00E1: return 0x91; // á
        case 0x00E2: return 0x92; // â
        case 0x00E4: return 0x93; // ä
        case 0x00E7: return 0x94; // ç
        case 0x00E8: return 0x95; // è
        case 0x00E9: return 0x96; // é
        case 0x00EA: return 0x97; // ê
        case 0x00EB: return 0x98; // ë
        case 0x00EF: return 0x99; // ï
        case 0x00F4: return 0x9A; // ô
        case 0x00F6: return 0x9B; // ö
        case 0x00F9: return 0x9C; // ù
        case 0x00FB: return 0x9D; // û
        case 0x00FC: return 0x9E; // ü
        case 0x2019: return '\''; // ’
        case 0x2018: return '\'';
        case 0x00A0: return ' ';
        default: return -1;
    }
}

// Pieces of a text: either one game glyph or one interface-font codepoint.
struct Piece {
    int glyph;         // 0x20-0xAB, or -1 for the interface font
    float advanceMul;  // ligature: the first half overlaps the second
    const char* begin; // interface font run (one codepoint)
    const char* end;
};

template <typename Fn> void ForEachPiece(const char* text, const char* textEnd, Fn&& fn) {
    const char* p = text;
    while (p < textEnd && *p != 0) {
        const char* start = p;
        unsigned int cp = 0;
        const int g = Decode(p, textEnd, cp);
        if (g >= 0) {
            fn(Piece{ g, 1.0f, start, p });
        } else if (cp == 0x0153 || cp == 0x0152) { // œ / Œ: written o + e like the game would
            const bool upper = cp == 0x0152;
            fn(Piece{ upper ? 'O' : 'o', kLigature, start, p });
            fn(Piece{ upper ? 'E' : 'e', 1.0f, start, p });
        } else {
            fn(Piece{ -1, 1.0f, start, p });
        }
    }
}

float GlyphAdvance(int glyph, float cell) {
    return sFontWidths[glyph - ' '] * cell / 16.0f * kSpacing;
}

int AtlasPx(float cell) {
    const int px = static_cast<int>(std::lround(cell * GetUiScale()));
    return std::clamp((px + 1) & ~1, 8, 128);
}

// Atlas texture for this pixel size, or nullptr (then asked for next frame).
ImTextureID Atlas(int px) {
    auto it = sAtlases.find(px);
    if (it == sAtlases.end()) {
        sAtlases[px] = AtlasState::Requested;
        return nullptr;
    }
    if (it->second != AtlasState::Ready) {
        return nullptr;
    }
    char name[24];
    std::snprintf(name, sizeof(name), "DST_FONT_%d", px);
    return Tex(name);
}

// Width of a text in logical pixels. size = interface font size it replaces.
float Width(ImFont* font, float size, const char* text, float scale) {
    const float cell = size * scale;
    float w = 0.0f;
    ForEachPiece(text, text + std::strlen(text), [&](const Piece& piece) {
        if (piece.glyph >= 0) {
            w += GlyphAdvance(piece.glyph, cell) * piece.advanceMul;
        } else {
            w += font->CalcTextSizeA(size, FLT_MAX, 0.0f, piece.begin, piece.end).x;
        }
    });
    return w;
}

// Draws like ImDrawList::AddText(font, size, pos, ...): same cap line centre,
// letters scaled by `scale`. Returns false if the atlas is not ready yet.
bool Draw(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text, float scale,
          const ImVec4* clip = nullptr) {
    const float cell = size * scale;
    const int px = AtlasPx(cell);
    ImTextureID atlas = Atlas(px);
    if (atlas == nullptr) {
        return false;
    }
    const float uiScale = GetUiScale();
    const float drawCell = px / uiScale; // exact texel size on screen
    // Cap centre of the interface font at this size, and of the game glyphs.
    const float ascent = font->Ascent * size / font->FontSize;
    const float capCentre = pos.y + ascent - kCapHeight * size * 0.5f;
    const float top = std::round((capCentre - (kCapTop + kCapHeight * 0.5f) * drawCell) * uiScale) / uiScale;
    if (clip != nullptr) {
        dl->PushClipRect(ImVec2(clip->x, clip->y), ImVec2(clip->z, clip->w), true);
    }
    float x = pos.x;
    const float uw = 1.0f / kAtlasCols;
    const float uh = 1.0f / kAtlasRows;
    ForEachPiece(text, text + std::strlen(text), [&](const Piece& piece) {
        if (piece.glyph >= 0) {
            const int idx = piece.glyph - ' ';
            if (piece.glyph != ' ') {
                const float gx = std::round(x * uiScale) / uiScale;
                const ImVec2 uv0((idx % kAtlasCols) * uw, (idx / kAtlasCols) * uh);
                dl->AddImage(atlas, ImVec2(gx, top), ImVec2(gx + drawCell, top + drawCell), uv0,
                             ImVec2(uv0.x + uw, uv0.y + uh), color);
            }
            x += GlyphAdvance(piece.glyph, cell) * piece.advanceMul;
        } else {
            dl->AddText(font, size, ImVec2(x, pos.y), color, piece.begin, piece.end);
            x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, piece.begin, piece.end).x;
        }
    });
    if (clip != nullptr) {
        dl->PopClipRect();
    }
    return true;
}

// One glyph of the game, as RGBA at its source size (HD pack or original).
bool LoadGlyph(int idx, std::vector<uint8_t>& rgba, int& w, int& h) {
    auto rm = Ship::Context::GetRawInstance()->GetResourceManager();
    const std::string path = fontTbl[idx];
    std::shared_ptr<Fast::Texture> tex;
    if (ResourceMgr_IsAltAssetsEnabled() && ResourceMgr_FileAltExists(path.c_str())) {
        const std::string altPath = "alt/" + (path.rfind("__OTR__", 0) == 0 ? path.substr(7) : path);
        tex = std::dynamic_pointer_cast<Fast::Texture>(rm->LoadResource(altPath, true));
        if (tex != nullptr && (tex->ImageData == nullptr || tex->Width == 0 ||
                               tex->ImageDataSize < static_cast<uint32_t>(tex->Width) * tex->Height * 4)) {
            tex = nullptr;
        }
        if (tex != nullptr) {
            w = tex->Width;
            h = tex->Height;
            rgba.assign(tex->ImageData, tex->ImageData + static_cast<size_t>(w) * h * 4);
            return true;
        }
    }
    tex = std::dynamic_pointer_cast<Fast::Texture>(rm->LoadResource(path, true));
    if (tex == nullptr || tex->ImageData == nullptr || tex->Width == 0) {
        return false;
    }
    w = tex->Width;
    h = tex->Height;
    const size_t count = static_cast<size_t>(w) * h;
    rgba.assign(count * 4, 0xFF);
    if ((tex->Flags & TEX_FLAG_LOAD_AS_RAW) != 0 && tex->ImageDataSize >= count * 4) {
        rgba.assign(tex->ImageData, tex->ImageData + count * 4);
    } else if (tex->ImageDataSize >= count / 2) { // I4: two pixels per byte, intensity = coverage
        for (size_t i = 0; i < count; i++) {
            const uint8_t b = tex->ImageData[i / 2];
            const uint8_t v = (i % 2 == 0) ? (b >> 4) : (b & 0x0F);
            rgba[i * 4 + 3] = static_cast<uint8_t>(v * 17);
        }
    } else {
        return false;
    }
    return true;
}

// Resamples a glyph to px x px: area average when shrinking (HD pack), nearest
// when growing (the original pixel font stays pixel art). Colour is weighted
// by coverage so the edges do not darken.
void Resample(const std::vector<uint8_t>& src, int w, int h, int px, std::vector<uint8_t>& atlas, int atlasW,
              int ox, int oy) {
    const float sx = static_cast<float>(w) / px;
    const float sy = static_cast<float>(h) / px;
    for (int y = 0; y < px; y++) {
        for (int x = 0; x < px; x++) {
            int xa = static_cast<int>(x * sx);
            int ya = static_cast<int>(y * sy);
            int xb = std::max(xa + 1, static_cast<int>((x + 1) * sx));
            int yb = std::max(ya + 1, static_cast<int>((y + 1) * sy));
            float acc[4] = { 0, 0, 0, 0 };
            int n = 0;
            for (int yy = ya; yy < yb && yy < h; yy++) {
                for (int xx = xa; xx < xb && xx < w; xx++) {
                    const uint8_t* p = &src[(static_cast<size_t>(yy) * w + xx) * 4];
                    const float a = p[3] / 255.0f;
                    acc[0] += p[0] * a;
                    acc[1] += p[1] * a;
                    acc[2] += p[2] * a;
                    acc[3] += a;
                    n++;
                }
            }
            uint8_t* out = &atlas[(static_cast<size_t>(oy + y) * atlasW + ox + x) * 4];
            for (int c = 0; c < 3; c++) {
                out[c] = acc[3] > 0.0f ? static_cast<uint8_t>(std::clamp(acc[c] / acc[3], 0.0f, 255.0f)) : 255;
            }
            out[3] = n > 0 ? static_cast<uint8_t>(std::clamp(acc[3] / n * 255.0f + 0.5f, 0.0f, 255.0f)) : 0;
        }
    }
}

void UploadAtlas(Fast::Fast3dGui* gui, int px, std::vector<std::vector<uint8_t>>& glyphs,
                 const std::vector<std::pair<int, int>>& sizes) {
    const int atlasW = px * kAtlasCols;
    const int atlasH = px * kAtlasRows;
    std::vector<uint8_t> atlas(static_cast<size_t>(atlasW) * atlasH * 4, 0);
    for (int i = 0; i < kGlyphCount; i++) {
        if (!glyphs[i].empty()) {
            Resample(glyphs[i], sizes[i].first, sizes[i].second, px, atlas, atlasW, (i % kAtlasCols) * px,
                     (i / kAtlasCols) * px);
        }
    }
    Fast::Texture tex;
    tex.Type = Fast::TextureType::RGBA32bpp;
    tex.Width = static_cast<uint16_t>(atlasW);
    tex.Height = static_cast<uint16_t>(atlasH);
    tex.ImageDataSize = static_cast<uint32_t>(atlas.size());
    auto buffer = std::make_shared<std::vector<char>>(atlas.begin(), atlas.end());
    tex.mImageBuffer = buffer;
    tex.ImageData = reinterpret_cast<uint8_t*>(buffer->data());
    char name[24];
    std::snprintf(name, sizeof(name), "DST_FONT_%d", px);
    gui->LoadGuiTexture(name, tex, "", ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
}

// Main GL context: builds the atlases asked for during the last frame.
void BuildRequestedAtlases(Fast::Fast3dGui* gui) {
    static std::vector<std::vector<uint8_t>> sGlyphs;
    static std::vector<std::pair<int, int>> sSizes;
    bool pending = false;
    for (const auto& entry : sAtlases) {
        pending |= entry.second == AtlasState::Requested;
    }
    if (!pending && sAvailable >= 0) {
        return;
    }
    if (sAvailable < 0) {
        sGlyphs.assign(kGlyphCount, {});
        sSizes.assign(kGlyphCount, { 0, 0 });
        for (int i = 0; i < kGlyphCount; i++) {
            if (!LoadGlyph(i, sGlyphs[i], sSizes[i].first, sSizes[i].second)) {
                sGlyphs[i].clear();
            }
        }
        sAvailable = sGlyphs['A' - ' '].empty() ? 0 : 1;
        if (sAvailable == 0) {
            __android_log_print(ANDROID_LOG_WARN, kLogTag, "Game font glyphs not found: interface font kept");
        }
    }
    for (auto& entry : sAtlases) {
        if (entry.second != AtlasState::Requested) {
            continue;
        }
        if (sAvailable == 0) {
            entry.second = AtlasState::Failed;
            continue;
        }
        UploadAtlas(gui, entry.first, sGlyphs, sSizes);
        entry.second = AtlasState::Ready;
    }
}

} // namespace GameFont

// Wrapped paragraph in the current ImGui font size and text colour (game font
// when it is on), like ImGui::TextWrapped("%s", text).
void WrappedText(const char* text) {
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    if (!GameFont::Enabled()) {
        ImGui::TextWrapped("%s", text);
        return;
    }
    const float scale = GameFont::RoleScale(size);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float wrapWidth = std::max(40.0f, ImGui::CalcWrapWidthForPos(start, 0.0f));
    const float lineHeight = size * std::max(1.0f, scale) * 1.22f;
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    // Greedy word wrap on spaces.
    std::vector<std::string> lines;
    std::string line;
    const char* p = text;
    while (*p != 0) {
        const char* wordEnd = p;
        while (*wordEnd != 0 && *wordEnd != ' ' && *wordEnd != '\n') {
            wordEnd++;
        }
        const std::string word(p, wordEnd);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && GameFont::Width(font, size, candidate.c_str(), scale) > wrapWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        p = wordEnd;
        if (*p == '\n') {
            lines.push_back(line);
            line.clear();
            p++;
        } else if (*p == ' ') {
            p++;
        }
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float y = start.y;
    bool drawn = true;
    for (const std::string& l : lines) {
        const ImVec2 pos(start.x, y + (lineHeight - size) * 0.5f);
        drawn &= GameFont::Draw(dl, font, size, ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, 200),
                                l.c_str(), scale);
        GameFont::Draw(dl, font, size, pos, color, l.c_str(), scale);
        y += lineHeight;
    }
    if (!drawn) {
        // Atlas not ready yet (first frame at this size): interface font once.
        ImGui::TextWrapped("%s", text);
        return;
    }
    ImGui::Dummy(ImVec2(wrapWidth, std::max(lineHeight, y - start.y)));
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

void DrawTabButton(const char* label, TrackerTab tab, float width, float height, bool notify = false) {
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
    const ImU32 labelColor = selected ? IM_COL32(255, 255, 120, 255) : IM_COL32(200, 200, 200, 255);
    // Game font: tab titles big, like the pause menu headings.
    constexpr float kTabTitleScale = 1.3f;
    const bool gameFont = GameFont::Enabled();
    const float labelWidth = gameFont ? GameFont::Width(font, fontSize, label, kTabTitleScale)
                                      : font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label).x;
    const float labelHeight = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label).y;
    const ImVec2 textPos(min.x + (width - labelWidth) * 0.5f, min.y + (height - labelHeight) * 0.5f);
    if (!gameFont ||
        !GameFont::Draw(drawList, font, fontSize, ImVec2(textPos.x + 2.0f, textPos.y + 2.0f), IM_COL32(0, 0, 0, 200),
                        label, kTabTitleScale)) {
        drawList->AddText(font, fontSize, ImVec2(textPos.x + 2.0f, textPos.y + 2.0f), IM_COL32(0, 0, 0, 200), label);
        drawList->AddText(font, fontSize, textPos, labelColor, label);
    } else {
        GameFont::Draw(drawList, font, fontSize, textPos, labelColor, label, kTabTitleScale);
    }
    if (selected) {
        DrawPauseCursor(drawList, min, max);
    } else if (notify) {
        // Something new to look at in this tab: a small pulsing Navi-blue dot.
        const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f);
        drawList->AddCircleFilled(ImVec2(max.x - 12.0f, min.y + 12.0f), 6.0f,
                                  IM_COL32(120, 230, 255, static_cast<int>(140 + 115 * pulse)));
    }
}

#include "DualScreenHintIndex.inc"
#include "DualScreenChecks.inc"
#include "DualScreenHintMap.inc"

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
        const float barHeight = textSize + 16.0f;
        const ImVec2 barMax(barMin.x + ImGui::GetContentRegionAvail().x, barMin.y + barHeight);
        DrawTextbox(drawList, barMin, barMax, IM_COL32(0, 0, 0, 200));

        const float textY = barMin.y + (barHeight - textSize) * 0.5f;
        const bool gameFont = GameFont::Enabled();
        auto shadowText = [&](ImVec2 pos, ImU32 color, const char* text) {
            if (gameFont && GameFont::Draw(drawList, font, textSize, ImVec2(pos.x + 2.0f, pos.y + 2.0f),
                                           IM_COL32(0, 0, 0, 220), text, 1.0f)) {
                GameFont::Draw(drawList, font, textSize, pos, color, text, 1.0f);
                return;
            }
            drawList->AddText(font, textSize, ImVec2(pos.x + 2.0f, pos.y + 2.0f), IM_COL32(0, 0, 0, 220), text);
            drawList->AddText(font, textSize, pos, color, text);
        };
        const std::string runLabel = std::string(ICON_FA_HOURGLASS_HALF " ") + runTime;
        shadowText(ImVec2(barMin.x + 20.0f, textY), IM_COL32(255, 255, 255, 255), runLabel.c_str());

        const ImU32 clockColor = isNight ? IM_COL32(150, 180, 255, 255) : IM_COL32(255, 220, 100, 255);
        const float iconSize = textSize;
        const float clockWidth = gameFont ? GameFont::Width(font, textSize, clock, 1.0f)
                                          : font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, clock).x;
        const float clockX = barMax.x - 20.0f - clockWidth;
        ImTextureID clockIcon = Tex("DST_CLOCK");
        if (clockIcon != nullptr) {
            drawList->AddImage(clockIcon, ImVec2(clockX - iconSize - 10.0f, textY),
                               ImVec2(clockX - 10.0f, textY + iconSize), ImVec2(0, 0), ImVec2(1, 1), clockColor);
        }
        shadowText(ImVec2(clockX, textY), clockColor, clock);
        ImGui::Dummy(ImVec2(0.0f, barHeight + 8.0f));
    }

    // The check data is followed on every frame, whatever the tab, so newly
    // reachable checks are noticed even while another tab is shown.
    const bool checksReady = Checks::Update();
    const bool hintsReady = HintIndex::Update() && checksReady;
    HintMap::Update(hintsReady);

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float tabWidth = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
    const float tabHeight = 44.0f;
    DrawTabButton(Label("Items", "Gegenstaende", "Objets"), TAB_ITEMS, tabWidth, tabHeight);
    ImGui::SameLine();
    DrawTabButton(Label("Hints", "Hinweise", "Indices"), TAB_HINTS, tabWidth, tabHeight,
                  hintsReady && HintMap::HasNews());
    ImGui::SameLine();
    DrawTabButton(Label("Checks", "Checks", "Checks"), TAB_CHECKS, tabWidth, tabHeight,
                  checksReady && Checks::HasFresh());
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const int tab = CVarGetInteger(CVAR_ENHANCEMENT("DualScreenTrackerTab"), TAB_ITEMS);
    if (tab == TAB_CHECKS) {
        Checks::Draw(checksReady);
    } else if (tab == TAB_HINTS) {
        HintMap::Draw(hintsReady);
    } else {
        // Everything fits on one page: no scrollbar on the item tracker.
        ImGui::BeginChild("##DualScreenItems", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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

namespace {

// Uploads an RGBA8 picture as a GUI texture.
void UploadRgba(Fast::Fast3dGui* gui, const char* name, std::vector<uint8_t> rgba, uint16_t width, uint16_t height) {
    Fast::Texture tex;
    tex.Type = Fast::TextureType::RGBA32bpp;
    tex.Width = width;
    tex.Height = height;
    tex.ImageDataSize = static_cast<uint32_t>(rgba.size());
    auto buffer = std::make_shared<std::vector<char>>(rgba.begin(), rgba.end());
    tex.mImageBuffer = buffer;
    tex.ImageData = reinterpret_cast<uint8_t*>(buffer->data());
    gui->LoadGuiTexture(name, tex, "", ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
}

// Halves a picture (2x2 averages) until it is at most maxWidth wide: the GUI
// samples textures at the nearest texel, so oversized HD art would shimmer.
void ShrinkRgba(std::vector<uint8_t>& rgba, uint16_t& width, uint16_t& height, int maxWidth) {
    while (width > maxWidth && width >= 2 && height >= 2) {
        const int w = width / 2;
        const int h = height / 2;
        std::vector<uint8_t> out(static_cast<size_t>(w) * h * 4);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                for (int c = 0; c < 4; c++) {
                    int sum = 0;
                    for (int dy = 0; dy < 2; dy++) {
                        for (int dx = 0; dx < 2; dx++) {
                            sum += rgba[((static_cast<size_t>(y) * 2 + dy) * width + (x * 2 + dx)) * 4 + c];
                        }
                    }
                    out[(static_cast<size_t>(y) * w + x) * 4 + c] = static_cast<uint8_t>(sum / 4);
                }
            }
        }
        rgba.swap(out);
        width = static_cast<uint16_t>(w);
        height = static_cast<uint16_t>(h);
    }
}

std::shared_ptr<Fast::Texture> LoadTextureExact(const std::string& path) {
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path, true);
    return std::dynamic_pointer_cast<Fast::Texture>(res);
}

// The pause menu world map (colour-indexed in the ROM, so decoded here), the
// area boxes that light up the current place, and the map dot.
void LoadWorldMapTextures(Fast::Fast3dGui* gui) {
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    bool mapLoaded = false;

    // HD texture pack version first, when one is active.
    if (ResourceMgr_IsAltAssetsEnabled() && ResourceMgr_FileAltExists(dgWorldMapImageTex)) {
        auto alt = LoadTextureExact("alt/textures/icon_item_field_static/gWorldMapImageTex");
        if (alt != nullptr && alt->ImageData != nullptr && alt->Type == Fast::TextureType::RGBA32bpp &&
            (alt->Flags & TEX_FLAG_LOAD_AS_RAW) != 0 && alt->Width > 0 && alt->Height > 0) {
            uint16_t w = alt->Width;
            uint16_t h = alt->Height;
            std::vector<uint8_t> rgba(alt->ImageData, alt->ImageData + static_cast<size_t>(w) * h * 4);
            ShrinkRgba(rgba, w, h, 1300);
            UploadRgba(gui, "DST_WORLD_MAP", std::move(rgba), w, h);
            mapLoaded = true;
        }
    }

    if (!mapLoaded) {
        auto image = LoadTextureExact(dgWorldMapImageTex);
        auto palette = LoadTextureExact(dgWorldMapImageTLUT);
        if (image != nullptr && palette != nullptr && image->ImageData != nullptr && palette->ImageData != nullptr &&
            image->Width > 0 && image->Height > 0 && palette->ImageDataSize >= 512) {
            const size_t count = static_cast<size_t>(image->Width) * image->Height;
            if (image->ImageDataSize >= count) {
                std::vector<uint8_t> rgba(count * 4);
                for (size_t i = 0; i < count; i++) {
                    const uint8_t index = image->ImageData[i];
                    const uint8_t b1 = palette->ImageData[index * 2 + 0];
                    const uint8_t b2 = palette->ImageData[index * 2 + 1];
                    rgba[i * 4 + 0] = static_cast<uint8_t>((b1 >> 3) * 0xFF / 0x1F);
                    rgba[i * 4 + 1] = static_cast<uint8_t>((((b1 & 7) << 2) | (b2 >> 6)) * 0xFF / 0x1F);
                    rgba[i * 4 + 2] = static_cast<uint8_t>(((b2 >> 1) & 0x1F) * 0xFF / 0x1F);
                    rgba[i * 4 + 3] = 0xFF;
                }
                UploadRgba(gui, "DST_WORLD_MAP", std::move(rgba), image->Width, image->Height);
            }
        }
    }

    static const char* const boxPaths[] = {
        dgWorldMapAreaBox1Tex, dgWorldMapAreaBox2Tex, dgWorldMapAreaBox3Tex, dgWorldMapAreaBox4Tex,
        dgWorldMapAreaBox5Tex, dgWorldMapAreaBox6Tex, dgWorldMapAreaBox7Tex, dgWorldMapAreaBox8Tex,
    };
    for (int i = 0; i < 8; i++) {
        char name[16];
        std::snprintf(name, sizeof(name), "DST_WM_BOX%d", i + 1);
        gui->LoadGuiTexture(name, boxPaths[i], "", white);
    }
    gui->LoadGuiTexture("DST_WM_DOT", dgWorldMapDotTex, "", white);
}

} // namespace

void DualScreenTracker_PrepareResources() {
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if (gui == nullptr) {
        return;
    }
    if (CVarGetInteger(CVAR_ENHANCEMENT("DualScreenGameFont"), 1) != 0) {
        GameFont::BuildRequestedAtlases(gui.get());
    }
    if (sTexturesLoaded) {
        return;
    }
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    gui->LoadGuiTexture("DST_MSG_BG", dgDefaultMessageBackgroundTex, "", white);
    gui->LoadGuiTexture("DST_CURSOR_TL", dgPauseMenuCursorTopLeftTex, "", white);
    gui->LoadGuiTexture("DST_CURSOR_TR", dgPauseMenuCursorTopRightTex, "", white);
    gui->LoadGuiTexture("DST_CURSOR_BL", dgPauseMenuCursorBottomLeftTex, "", white);
    gui->LoadGuiTexture("DST_CURSOR_BR", dgPauseMenuCursorBottomRightTex, "", white);
    gui->LoadGuiTexture("DST_CLOCK", dgClockIconTex, "", white);
    LoadWorldMapTextures(gui.get());
    sTexturesLoaded = true;
}

// Hint journal (randomizer_hint_tracker.cpp, bottom screen): paragraphs and
// labels of its ImGui widgets in the game's font.
void DualScreenTracker_WrappedText(const char* text) {
    WrappedText(text);
}

void DualScreenTracker_LabelText(ImVec2 min, ImVec2 max, const char* text, ImU32 color, float offsetX, bool centred) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    const bool gameFont = GameFont::Enabled();
    const float scale = GameFont::RoleScale(size);
    const float width = gameFont ? GameFont::Width(font, size, text, scale)
                                 : font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
    const float x = centred ? (min.x + max.x - width) * 0.5f : min.x + offsetX;
    // Line top such that the cap line sits in the middle of the frame.
    const float ascent = font->Ascent * size / font->FontSize;
    const float y = (min.y + max.y) * 0.5f - ascent + GameFont::kCapHeight * size * 0.5f;
    if (gameFont && GameFont::Draw(dl, font, size, ImVec2(x + 1.5f, y + 1.5f), IM_COL32(0, 0, 0, 200), text, scale)) {
        GameFont::Draw(dl, font, size, ImVec2(x, y), color, text, scale);
        return;
    }
    dl->AddText(font, size, ImVec2(x, y), color, text);
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
