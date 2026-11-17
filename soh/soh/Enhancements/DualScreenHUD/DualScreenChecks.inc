// Bottom-screen "Checks" tab (Thor build). Included inside the anonymous
// namespace of DualScreenTracker.cpp, after the drawing helpers.
//
// Built on the SoH Check Tracker data: only the checks the seed's settings
// actually shuffle are listed, they tick themselves when collected, and the
// logic (with the items the player has) tells which ones can be done now.
//
// Overview: the world grouped by medallion region (Forest, Fire, Water,
// Shadow, Spirit, Light), one card per area. A jump bar of medallions moves
// between regions. Tapping a card opens the area's list of checks.
//
// Colour code (fixed, independent of the region colours):
//   Navi blue  = can be done now with your items, at your current age ("to do")
//   lavender + hourglass = reachable too, but as the other age (the logic goes
//                through the Temple of Time: become adult / child first)
//   grey + lock = not reachable yet
//   gold tick, dimmed and struck out = done, no need to go back
//   pulsing NEW = became reachable recently (just got bombs, a song...)

namespace Checks {

constexpr ImU32 kColTodo = IM_COL32(120, 230, 255, 255);
constexpr ImU32 kColLocked = IM_COL32(125, 125, 140, 255);
constexpr ImU32 kColOtherAge = IM_COL32(195, 155, 255, 255);
constexpr ImU32 kColDone = IM_COL32(230, 190, 60, 255);
constexpr ImU32 kColDoneText = IM_COL32(120, 120, 130, 255);
constexpr ImU32 kColText = IM_COL32(240, 240, 240, 255);
constexpr ImU32 kColSubText = IM_COL32(170, 170, 185, 255);
constexpr ImU32 kColSeen = IM_COL32(150, 200, 255, 255);

// How long a newly reachable check stays highlighted (seconds of real time).
constexpr double kFreshSeconds = 180.0;

struct AreaDef {
    RandomizerCheckArea area;
    const char* fr;
};

struct RegionDef {
    const char* en;
    const char* fr;
    const char* medallion;
    ImU32 color;
    std::vector<AreaDef> areas;
};

const std::vector<RegionDef>& Regions() {
    static const std::vector<RegionDef> regions = {
        { "Forest",
          "Forêt",
          "QUEST_MEDALLION_FOREST",
          IM_COL32(90, 200, 90, 255),
          {
              { RCAREA_KOKIRI_FOREST, "Forêt Kokiri" },
              { RCAREA_LOST_WOODS, "Bois Perdus" },
              { RCAREA_SACRED_FOREST_MEADOW, "Bosquet Sacré" },
              { RCAREA_DEKU_TREE, "Arbre Mojo" },
              { RCAREA_FOREST_TEMPLE, "Temple de la Forêt" },
          } },
        { "Fire",
          "Feu",
          "QUEST_MEDALLION_FIRE",
          IM_COL32(235, 95, 65, 255),
          {
              { RCAREA_DEATH_MOUNTAIN_TRAIL, "Chemin du Péril" },
              { RCAREA_GORON_CITY, "Village Goron" },
              { RCAREA_DEATH_MOUNTAIN_CRATER, "Cratère du Péril" },
              { RCAREA_DODONGOS_CAVERN, "Caverne Dodongo" },
              { RCAREA_FIRE_TEMPLE, "Temple du Feu" },
          } },
        { "Water",
          "Eau",
          "QUEST_MEDALLION_WATER",
          IM_COL32(70, 140, 255, 255),
          {
              { RCAREA_ZORAS_RIVER, "Rivière Zora" },
              { RCAREA_ZORAS_DOMAIN, "Domaine Zora" },
              { RCAREA_ZORAS_FOUNTAIN, "Fontaine Zora" },
              { RCAREA_LAKE_HYLIA, "Lac Hylia" },
              { RCAREA_JABU_JABUS_BELLY, "Ventre de Jabu-Jabu" },
              { RCAREA_ICE_CAVERN, "Caverne Polaire" },
              { RCAREA_WATER_TEMPLE, "Temple de l'Eau" },
          } },
        { "Shadow",
          "Ombre",
          "QUEST_MEDALLION_SHADOW",
          IM_COL32(175, 115, 235, 255),
          {
              { RCAREA_KAKARIKO_VILLAGE, "Village Cocorico" },
              { RCAREA_GRAVEYARD, "Cimetière" },
              { RCAREA_BOTTOM_OF_THE_WELL, "Puits" },
              { RCAREA_SHADOW_TEMPLE, "Temple de l'Ombre" },
          } },
        { "Spirit",
          "Esprit",
          "QUEST_MEDALLION_SPIRIT",
          IM_COL32(245, 160, 50, 255),
          {
              { RCAREA_GERUDO_VALLEY, "Vallée Gerudo" },
              { RCAREA_GERUDO_FORTRESS, "Forteresse Gerudo" },
              { RCAREA_WASTELAND, "Désert Hanté" },
              { RCAREA_DESERT_COLOSSUS, "Colosse du Désert" },
              { RCAREA_GERUDO_TRAINING_GROUND, "Gymnase Gerudo" },
              { RCAREA_SPIRIT_TEMPLE, "Temple de l'Esprit" },
          } },
        { "Light",
          "Lumière",
          "QUEST_MEDALLION_LIGHT",
          IM_COL32(240, 215, 80, 255),
          {
              { RCAREA_HYRULE_FIELD, "Plaine d'Hyrule" },
              { RCAREA_MARKET, "Place du Marché" },
              { RCAREA_HYRULE_CASTLE, "Château d'Hyrule" },
              { RCAREA_LON_LON_RANCH, "Ranch Lon Lon" },
              { RCAREA_GANONS_CASTLE, "Château de Ganon" },
          } },
    };
    return regions;
}

enum class State { Done, Skipped, Todo, OtherAge, Locked, Unknown };

struct AreaStats {
    int total = 0;
    int done = 0;      // collected or skipped
    int todo = 0;      // reachable now (or simply remaining when logic is off)
    int otherAge = 0;  // reachable, but as the other age
    int fresh = 0;     // reachable since a short while
    bool spoiled = true;
};

struct RegionStats {
    int total = 0;
    int done = 0;
    int todo = 0;
    int otherAge = 0;
    int fresh = 0;
};

std::unordered_map<int, AreaStats> sAreaStats;
std::vector<RegionStats> sRegionStats;
std::vector<uint8_t> sWasTodo;
std::unordered_map<int, double> sFreshSince;
bool sBaselineSet = false;
bool sStatsDirty = true;
int sFrameCounter = 0;
int sLastAge = -1;

RandomizerCheckArea sDetailArea = RCAREA_INVALID;
bool sDetailJustOpened = false;
RandomizerCheckArea sFollowedArea = RCAREA_INVALID;
int sScrollToRegion = -1;
RandomizerCheck sExpandedCheck = RC_UNKNOWN_CHECK;

bool IsFrench() {
    return CVarGetInteger(CVAR_SETTING("Languages"), 0) == 2;
}

bool IsAdultNow() {
    return LINK_IS_ADULT;
}

// "adult" when Link is a child now, and the other way round.
const char* OtherAgeLabel() {
    return IsAdultNow() ? Label("as child", "als Kind", "en enfant") : Label("as adult", "als Erwachsener", "en adulte");
}

std::string AreaName(const AreaDef& def) {
    return IsFrench() ? std::string(def.fr) : RandomizerCheckObjects::GetRCAreaName(def.area);
}

State GetState(RandomizerCheck rc) {
    if (rc == RC_LINKS_POCKET) {
        return State::Done;
    }
    auto itemLoc = Rando::Context::GetInstance()->GetItemLocation(rc);
    if (itemLoc->HasObtained()) {
        return State::Done;
    }
    if (itemLoc->GetIsSkipped()) {
        return State::Skipped;
    }
    if (!CheckTracker::IsLogicShown()) {
        return State::Unknown;
    }
    if (!itemLoc->IsAvailable()) {
        return State::Locked;
    }
    // Reachable: at the current age, or only after going through the Temple
    // of Time? (Unknown ages = 0: keep it as plain "to do".)
    const uint8_t ages = CheckTracker::GetCheckReachAges(rc);
    const uint8_t now = IsAdultNow() ? CheckTracker::CHECK_AGE_ADULT : CheckTracker::CHECK_AGE_CHILD;
    return ages != 0 && (ages & now) == 0 ? State::OtherAge : State::Todo;
}

bool IsFresh(RandomizerCheck rc) {
    auto it = sFreshSince.find(static_cast<int>(rc));
    return it != sFreshSince.end() && ImGui::GetTime() - it->second < kFreshSeconds;
}

void Reset() {
    sAreaStats.clear();
    sRegionStats.clear();
    sWasTodo.clear();
    sFreshSince.clear();
    sBaselineSet = false;
    sStatsDirty = true;
    sLastAge = -1;
    sDetailArea = RCAREA_INVALID;
    sFollowedArea = RCAREA_INVALID;
    sExpandedCheck = RC_UNKNOWN_CHECK;
}

void RecomputeStats() {
    const double now = ImGui::GetTime();
    const bool mqSpoilers = CVarGetInteger(CVAR_TRACKER_CHECK("MQSpoilers"), 0) != 0;
    if (sWasTodo.size() != RC_MAX) {
        sWasTodo.assign(RC_MAX, 0);
    }
    sRegionStats.assign(Regions().size(), RegionStats());
    for (size_t r = 0; r < Regions().size(); r++) {
        for (const AreaDef& def : Regions()[r].areas) {
            AreaStats stats;
            stats.spoiled = mqSpoilers || CheckTracker::IsAreaSpoiled(def.area);
            for (RandomizerCheck rc : CheckTracker::GetChecksForArea(def.area)) {
                if (rc != RC_LINKS_POCKET && !CheckTracker::IsVisibleInCheckTracker(rc)) {
                    continue;
                }
                stats.total++;
                const State state = GetState(rc);
                const bool todo = state == State::Todo || state == State::Unknown;
                if (state == State::Done || state == State::Skipped) {
                    stats.done++;
                } else if (todo) {
                    stats.todo++;
                } else if (state == State::OtherAge) {
                    stats.otherAge++;
                }
                // Newly reachable: highlight it for a while (only once a first
                // baseline exists, so loading a save doesn't light up everything).
                // Changing age only moves checks between blue and lavender: not news.
                const bool reachable = state == State::Todo || state == State::OtherAge;
                if (reachable && !sWasTodo[rc] && sBaselineSet) {
                    sFreshSince[static_cast<int>(rc)] = now;
                }
                if (!reachable) {
                    sFreshSince.erase(static_cast<int>(rc));
                }
                sWasTodo[rc] = reachable ? 1 : 0;
                if (reachable && IsFresh(rc)) {
                    stats.fresh++;
                }
            }
            sAreaStats[def.area] = stats;
            RegionStats& region = sRegionStats[r];
            region.total += stats.total;
            region.done += stats.done;
            region.todo += stats.todo;
            region.otherAge += stats.otherAge;
            region.fresh += stats.fresh;
        }
    }
    if (CheckTracker::IsLogicShown()) {
        sBaselineSet = true;
    }
}

const AreaStats& StatsFor(RandomizerCheckArea area) {
    static const AreaStats empty;
    auto it = sAreaStats.find(area);
    return it == sAreaStats.end() ? empty : it->second;
}

int RegionOf(RandomizerCheckArea area) {
    for (size_t r = 0; r < Regions().size(); r++) {
        for (const AreaDef& def : Regions()[r].areas) {
            if (def.area == area) {
                return static_cast<int>(r);
            }
        }
    }
    return -1;
}

const AreaDef* AreaDefFor(RandomizerCheckArea area) {
    const int r = RegionOf(area);
    if (r < 0) {
        return nullptr;
    }
    for (const AreaDef& def : Regions()[r].areas) {
        if (def.area == area) {
            return &def;
        }
    }
    return nullptr;
}

// Called every frame from DrawTrackerUi, whatever the tab.
bool Update() {
    if (!CheckTracker::EmbeddedBegin()) {
        if (sBaselineSet || !sAreaStats.empty()) {
            Reset();
        }
        return false;
    }
    // Becoming child or adult changes everything that can be reached.
    const int age = gSaveContext.linkAge;
    if (sLastAge != -1 && age != sLastAge) {
        CheckTracker::RecalculateAvailableChecks();
        sStatsDirty = true;
    }
    sLastAge = age;

    if (sStatsDirty || ++sFrameCounter >= 10) {
        sFrameCounter = 0;
        sStatsDirty = false;
        RecomputeStats();
    }

    // Follow the player: entering an area opens its list.
    const RandomizerCheckArea current = CheckTracker::GetCurrentTrackerArea();
    if (current != sFollowedArea) {
        sFollowedArea = current;
        if (CVarGetInteger(CVAR_ENHANCEMENT("DualScreenChecksFollow"), 1) && current != RCAREA_INVALID &&
            StatsFor(current).total > 0 && AreaDefFor(current) != nullptr) {
            sDetailArea = current;
            sDetailJustOpened = true;
            sExpandedCheck = RC_UNKNOWN_CHECK;
        }
    }
    return true;
}

bool HasFresh() {
    for (const RegionStats& region : sRegionStats) {
        if (region.fresh > 0) {
            return true;
        }
    }
    return false;
}

// ---- drawing ----

void ShadowText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text,
                float clipRight = FLT_MAX) {
    const ImVec4 clip(pos.x - 4.0f, pos.y - 4.0f, clipRight, pos.y + size * 2.0f);
    dl->AddText(font, size, ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, 200), text, nullptr, 0.0f,
                clipRight == FLT_MAX ? nullptr : &clip);
    dl->AddText(font, size, pos, color, text, nullptr, 0.0f, clipRight == FLT_MAX ? nullptr : &clip);
}

float TextWidth(ImFont* font, float size, const char* text) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
}

ImU32 WithAlpha(ImU32 color, int alpha) {
    return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

float Pulse(float speed = 5.0f) {
    return 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * speed);
}

void DrawMedallion(ImDrawList* dl, const RegionDef& region, ImVec2 min, float size, bool dim = false) {
    ImTextureID tex = Tex(region.medallion);
    if (tex != nullptr) {
        dl->AddImage(tex, min, ImVec2(min.x + size, min.y + size), ImVec2(0, 0), ImVec2(1, 1),
                     dim ? IM_COL32(255, 255, 255, 110) : IM_COL32_WHITE);
    } else {
        dl->AddCircleFilled(ImVec2(min.x + size * 0.5f, min.y + size * 0.5f), size * 0.45f,
                            dim ? WithAlpha(region.color, 110) : region.color);
    }
}

void OpenOverview(int region) {
    sDetailArea = RCAREA_INVALID;
    sScrollToRegion = region;
    sExpandedCheck = RC_UNKNOWN_CHECK;
}

// Summary line + medallion jump bar, shown on both views.
void DrawHeader() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* big = BigFont();
    const float width = ImGui::GetContentRegionAvail().x;

    // Global progress.
    int total = 0, done = 0, todo = 0, otherAge = 0;
    for (const RegionStats& region : sRegionStats) {
        total += region.total;
        done += region.done;
        todo += region.todo;
        otherAge += region.otherAge;
    }
    {
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const float h = 26.0f;
        char left[48];
        std::snprintf(left, sizeof(left), "%d / %d", done, total);
        ShadowText(dl, big, 18.0f, ImVec2(min.x + 4.0f, min.y + 3.0f), kColText, left);
        char right[64];
        const bool logic = CheckTracker::IsLogicShown();
        // Both ages together: the list of a zone tells them apart (colour).
        std::snprintf(right, sizeof(right), "%s %d %s", logic ? ICON_FA_ARROW_CIRCLE_RIGHT : ICON_FA_CIRCLE_O,
                      todo + otherAge, logic ? Label("to do", "offen", "à faire") : Label("left", "offen", "restants"));
        const float rightW = TextWidth(big, 18.0f, right);
        ShadowText(dl, big, 18.0f, ImVec2(min.x + width - rightW - 4.0f, min.y + 3.0f), logic ? kColTodo : kColText,
                   right);
        // Progress bar between the two labels.
        const float barX0 = min.x + TextWidth(big, 18.0f, left) + 18.0f;
        const float barX1 = min.x + width - rightW - 18.0f;
        if (barX1 > barX0 + 20.0f) {
            const float y = min.y + h * 0.5f - 3.0f;
            dl->AddRectFilled(ImVec2(barX0, y), ImVec2(barX1, y + 7.0f), IM_COL32(45, 45, 60, 255), 4.0f);
            const float frac = total > 0 ? static_cast<float>(done) / static_cast<float>(total) : 0.0f;
            if (frac > 0.0f) {
                dl->AddRectFilled(ImVec2(barX0, y), ImVec2(barX0 + (barX1 - barX0) * frac, y + 7.0f), kColDone, 4.0f);
            }
        }
        ImGui::Dummy(ImVec2(width, h));
    }

    // Medallion jump bar: one button per region, with what is left to do there.
    const float spacing = 6.0f;
    const size_t count = Regions().size();
    const float buttonW = (width - spacing * (count - 1)) / count;
    const float buttonH = 44.0f;
    const int currentRegion = RegionOf(CheckTracker::GetCurrentTrackerArea());
    for (size_t r = 0; r < count; r++) {
        const RegionDef& region = Regions()[r];
        const RegionStats stats = r < sRegionStats.size() ? sRegionStats[r] : RegionStats();
        if (r > 0) {
            ImGui::SameLine(0.0f, spacing);
        }
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + buttonW, min.y + buttonH);
        ImGui::PushID(static_cast<int>(r));
        if (ImGui::InvisibleButton("##region", ImVec2(buttonW, buttonH))) {
            OpenOverview(static_cast<int>(r));
        }
        ImGui::PopID();
        const bool complete = stats.total > 0 && stats.done == stats.total;
        dl->AddRectFilled(min, max, IM_COL32(14, 14, 24, 235), 8.0f);
        dl->AddRect(min, max, WithAlpha(region.color, complete ? 90 : 200), 8.0f, 0, 2.0f);
        const float icon = 30.0f;
        DrawMedallion(dl, region, ImVec2(min.x + 6.0f, min.y + (buttonH - icon) * 0.5f), icon, complete);
        if (stats.fresh > 0) {
            // Something just became reachable here: Navi at the medallion's corner.
            DrawNavi(dl, ImVec2(min.x + 6.0f + icon, min.y + (buttonH - icon) * 0.5f),
                     static_cast<int>(130 + 125 * Pulse(6.0f)));
        }
        // Badge: an optional icon (check, hourglass) at 18 px, the number bigger.
        const float iconSize = 18.0f;
        const float numberSize = 24.0f;
        const char* badgeIcon = nullptr;
        char number[16] = "";
        ImU32 badgeColor = kColTodo;
        if (complete) {
            badgeIcon = ICON_FA_CHECK;
            badgeColor = kColDone;
        } else if (stats.todo == 0 && stats.otherAge > 0) {
            // Same counter as "to do" above (both ages); in lavender when only
            // the other age has something to do here.
            badgeIcon = ICON_FA_HOURGLASS_HALF;
            std::snprintf(number, sizeof(number), "%d", stats.otherAge);
            badgeColor = kColOtherAge;
        } else {
            std::snprintf(number, sizeof(number), "%d", stats.todo + stats.otherAge);
            if (stats.todo == 0) {
                badgeColor = kColLocked;
            }
        }
        float x = max.x - 7.0f;
        if (number[0] != '\0') {
            x -= TextWidth(big, numberSize, number);
            ShadowText(dl, big, numberSize, ImVec2(x, min.y + (buttonH - numberSize) * 0.5f - 1.0f), badgeColor,
                       number);
        }
        if (badgeIcon != nullptr) {
            x -= TextWidth(big, iconSize, badgeIcon) + (number[0] != '\0' ? 4.0f : 0.0f);
            ShadowText(dl, big, iconSize, ImVec2(x, min.y + (buttonH - iconSize) * 0.5f - 1.0f), badgeColor,
                       badgeIcon);
        }
        if (static_cast<int>(r) == currentRegion) {
            // Where Link is: a small marker under the medallion.
            dl->AddTriangleFilled(ImVec2(min.x + 21.0f - 5.0f, max.y - 1.0f), ImVec2(min.x + 21.0f + 5.0f, max.y - 1.0f),
                                  ImVec2(min.x + 21.0f, max.y - 7.0f), kColText);
        }
    }
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
}

void DrawAreaCard(const RegionDef& region, const AreaDef& def, float w, float h) {
    const AreaStats& stats = StatsFor(def.area);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* big = BigFont();
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + w, min.y + h);
    ImGui::PushID(static_cast<int>(def.area));
    if (ImGui::InvisibleButton("##area", ImVec2(w, h))) {
        sDetailArea = def.area;
        sDetailJustOpened = true;
        sExpandedCheck = RC_UNKNOWN_CHECK;
    }
    ImGui::PopID();

    const bool complete = stats.total > 0 && stats.done == stats.total;
    const bool here = CheckTracker::GetCurrentTrackerArea() == def.area;
    dl->AddRectFilled(min, max, complete ? IM_COL32(16, 16, 22, 230) : IM_COL32(28, 28, 44, 245), 8.0f);
    dl->AddRectFilled(min, ImVec2(min.x + 6.0f, max.y), WithAlpha(region.color, complete ? 90 : 255), 8.0f,
                      ImDrawFlags_RoundCornersLeft);

    // Name (with a marker when Link is here) and done/total.
    std::string name = AreaName(def);
    if (here) {
        name = std::string(ICON_FA_MAP_MARKER " ") + name;
    }
    char counts[24];
    std::snprintf(counts, sizeof(counts), "%d/%d", stats.done, stats.total);
    const float countsW = TextWidth(big, 15.0f, counts);
    ShadowText(dl, big, 15.0f, ImVec2(max.x - countsW - 8.0f, min.y + 9.0f), kColSubText, counts);
    ShadowText(dl, big, 18.0f, ImVec2(min.x + 14.0f, min.y + 7.0f), complete ? kColDoneText : kColText, name.c_str(),
               max.x - countsW - 14.0f);

    // Status line.
    char status[64];
    ImU32 statusColor = kColText;
    const bool logic = CheckTracker::IsLogicShown();
    if (!stats.spoiled) {
        std::snprintf(status, sizeof(status), "???");
        statusColor = kColLocked;
    } else if (complete) {
        std::snprintf(status, sizeof(status), "%s %s", ICON_FA_CHECK, Label("Done", "Fertig", "Terminé"));
        statusColor = kColDone;
    } else if (!logic) {
        std::snprintf(status, sizeof(status), "%d %s", stats.todo, Label("left", "offen", "restants"));
    } else if (stats.todo > 0) {
        std::snprintf(status, sizeof(status), "%s %d %s", ICON_FA_ARROW_CIRCLE_RIGHT, stats.todo + stats.otherAge,
                      Label("to do", "offen", "à faire"));
        statusColor = kColTodo;
    } else if (stats.otherAge > 0) {
        std::snprintf(status, sizeof(status), "%s %d %s", ICON_FA_HOURGLASS_HALF, stats.otherAge, OtherAgeLabel());
        statusColor = kColOtherAge;
    } else {
        std::snprintf(status, sizeof(status), "%s %d %s", ICON_FA_LOCK, stats.total - stats.done,
                      Label("locked", "gesperrt", "bloqués"));
        statusColor = kColLocked;
    }
    ShadowText(dl, big, 16.0f, ImVec2(min.x + 14.0f, min.y + 32.0f), statusColor, status);
    if (logic && stats.spoiled && !complete && stats.todo > 0 && stats.otherAge > 0) {
        // How many of them need the other age, in lavender.
        char age[32];
        std::snprintf(age, sizeof(age), "%s %s %d", Label("incl.", "davon", "dont"), ICON_FA_HOURGLASS_HALF,
                      stats.otherAge);
        ShadowText(dl, big, 16.0f, ImVec2(min.x + 14.0f + TextWidth(big, 16.0f, status) + 10.0f, min.y + 32.0f),
                   kColOtherAge, age);
    }
    float tagRight = max.x - 8.0f;
    if (stats.spoiled && stats.fresh > 0) {
        char fresh[32];
        std::snprintf(fresh, sizeof(fresh), "+%d %s", stats.fresh, Label("NEW", "NEU", "NOUV."));
        const float freshW = TextWidth(big, 15.0f, fresh);
        ShadowText(dl, big, 15.0f, ImVec2(max.x - freshW - 8.0f, min.y + 33.0f),
                   WithAlpha(kColTodo, static_cast<int>(150 + 105 * Pulse(6.0f))), fresh);
        tagRight -= freshW + 8.0f;
    }
    // What the hints already read say about this area (Hints tab colours).
    {
        const HintIndex::ZoneInfo& hints = HintIndex::Get(def.area);
        const int pending = hints.PendingItems();
        if (pending > 0) {
            char star[16];
            std::snprintf(star, sizeof(star), ICON_FA_STAR "%d", pending);
            const float w = TextWidth(big, 14.0f, star);
            ShadowText(dl, big, 14.0f, ImVec2(tagRight - w, min.y + 34.0f), IM_COL32(255, 215, 90, 255), star);
            tagRight -= w + 8.0f;
        }
        if (hints.foolish || hints.woth) {
            const char* tag = hints.foolish ? Label("Foolish", "Unnütz", "Inutile") : Label("Hero", "Held", "Héros");
            const ImU32 color = hints.foolish ? IM_COL32(255, 150, 180, 255) : IM_COL32(100, 180, 255, 255);
            const float w = TextWidth(big, 13.0f, tag);
            const ImVec2 tMin(tagRight - w - 12.0f, min.y + 32.0f);
            dl->AddRectFilled(tMin, ImVec2(tagRight, tMin.y + 20.0f), WithAlpha(color, 50), 10.0f);
            dl->AddRect(tMin, ImVec2(tagRight, tMin.y + 20.0f), WithAlpha(color, 170), 10.0f, 0, 1.0f);
            ShadowText(dl, big, 13.0f, ImVec2(tMin.x + 6.0f, tMin.y + 3.0f), color, tag);
        }
    }

    // Progress bar in the region colour.
    const float barY = max.y - 9.0f;
    const float barX0 = min.x + 14.0f;
    const float barX1 = max.x - 10.0f;
    dl->AddRectFilled(ImVec2(barX0, barY), ImVec2(barX1, barY + 4.0f), IM_COL32(50, 50, 62, 255), 2.0f);
    if (stats.total > 0 && stats.done > 0) {
        const float frac = static_cast<float>(stats.done) / static_cast<float>(stats.total);
        dl->AddRectFilled(ImVec2(barX0, barY), ImVec2(barX0 + (barX1 - barX0) * frac, barY + 4.0f),
                          WithAlpha(region.color, complete ? 120 : 255), 2.0f);
    }
    if (here) {
        DrawPauseCursor(dl, min, max);
    }
}

void DrawOverview() {
    const bool scrollRequested = sScrollToRegion >= 0;
    ImGui::BeginChild("##ChecksOverview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* huge = HugeFont();
    ImFont* big = BigFont();
    const float spacing = 8.0f;
    for (size_t r = 0; r < Regions().size(); r++) {
        const RegionDef& region = Regions()[r];
        const RegionStats stats = r < sRegionStats.size() ? sRegionStats[r] : RegionStats();
        if (stats.total == 0) {
            continue;
        }
        if (scrollRequested && sScrollToRegion == static_cast<int>(r)) {
            ImGui::SetScrollHereY(0.0f);
            sScrollToRegion = -1;
        }
        // Region header strip.
        const float width = ImGui::GetContentRegionAvail().x;
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const float h = 34.0f;
        dl->AddRectFilledMultiColor(min, ImVec2(min.x + width, min.y + h), WithAlpha(region.color, 120),
                                    WithAlpha(region.color, 15), WithAlpha(region.color, 15),
                                    WithAlpha(region.color, 120));
        DrawMedallion(dl, region, ImVec2(min.x + 6.0f, min.y + 4.0f), 26.0f);
        ShadowText(dl, huge, 22.0f, ImVec2(min.x + 40.0f, min.y + 5.0f), kColText,
                   IsFrench() ? region.fr : region.en);
        char right[48];
        std::snprintf(right, sizeof(right), "%d/%d", stats.done, stats.total);
        const float rightW = TextWidth(big, 16.0f, right);
        ShadowText(dl, big, 16.0f, ImVec2(min.x + width - rightW - 8.0f, min.y + 9.0f), kColSubText, right);
        ImGui::Dummy(ImVec2(width, h + 4.0f));

        // Area cards, two per row.
        const float cardW = (width - spacing) * 0.5f;
        const float cardH = 62.0f;
        int column = 0;
        for (const AreaDef& def : region.areas) {
            if (StatsFor(def.area).total == 0) {
                continue;
            }
            if (column == 1) {
                ImGui::SameLine(0.0f, spacing);
            }
            DrawAreaCard(region, def, cardW, cardH);
            column = 1 - column;
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
    }
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::EndChild();
}

struct Row {
    RandomizerCheck rc;
    State state;
    bool fresh;
};

int StateRank(const Row& row) {
    if (row.fresh && row.state != State::OtherAge) {
        return 0;
    }
    switch (row.state) {
        case State::Todo:
        case State::Unknown:
            return 1;
        case State::OtherAge:
            return 2;
        case State::Locked:
            return 3;
        case State::Skipped:
            return 4;
        case State::Done:
        default:
            return 5;
    }
}

void DrawCheckRow(const Row& row, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* big = BigFont();
    const std::string label = CheckTracker::GetCheckLabel(row.rc);
    const std::string itemText = row.rc == RC_LINKS_POCKET ? std::string() : CheckTracker::GetCheckItemText(row.rc);
    const bool expanded = sExpandedCheck == row.rc;
    const float baseH = itemText.empty() ? 40.0f : 58.0f;
    const float actionH = 50.0f;
    const ImVec2 min = ImGui::GetCursorScreenPos();

    ImGui::PushID(static_cast<int>(row.rc));
    if (ImGui::InvisibleButton("##row", ImVec2(width, baseH))) {
        sExpandedCheck = expanded ? RC_UNKNOWN_CHECK : row.rc;
    }
    const ImVec2 max(min.x + width, min.y + baseH);

    ImU32 accent = kColText;
    ImU32 nameColor = kColText;
    const char* icon = ICON_FA_CIRCLE_O;
    bool strike = false;
    switch (row.state) {
        case State::Todo:
            accent = kColTodo;
            icon = ICON_FA_ARROW_CIRCLE_RIGHT;
            break;
        case State::Unknown:
            accent = kColText;
            icon = ICON_FA_CIRCLE_O;
            break;
        case State::OtherAge:
            accent = kColOtherAge;
            nameColor = IM_COL32(222, 214, 240, 255);
            icon = ICON_FA_HOURGLASS_HALF;
            break;
        case State::Locked:
            accent = kColLocked;
            nameColor = IM_COL32(165, 165, 178, 255);
            icon = ICON_FA_LOCK;
            break;
        case State::Skipped:
            accent = kColLocked;
            nameColor = kColDoneText;
            icon = ICON_FA_BAN;
            strike = true;
            break;
        case State::Done:
            accent = kColDone;
            nameColor = kColDoneText;
            icon = ICON_FA_CHECK;
            strike = true;
            break;
    }
    if (row.fresh) {
        accent = WithAlpha(accent, static_cast<int>(150 + 105 * Pulse(6.0f)));
    }

    const bool dim = row.state == State::Done || row.state == State::Skipped;
    dl->AddRectFilled(min, max, dim ? IM_COL32(14, 14, 20, 200) : IM_COL32(26, 26, 40, 235), 6.0f);
    dl->AddRectFilled(min, ImVec2(min.x + 5.0f, max.y), accent, 6.0f, ImDrawFlags_RoundCornersLeft);
    if (expanded) {
        dl->AddRect(min, max, IM_COL32(255, 255, 120, 200), 6.0f, 0, 2.0f);
    }

    // State icon, a bit larger than the text and centred in its column.
    const float iconSize = 23.0f;
    const float iconW = TextWidth(big, iconSize, icon);
    ShadowText(dl, big, iconSize, ImVec2(min.x + 26.0f - iconW * 0.5f, min.y + 7.0f), accent, icon);
    const float nameX = min.x + 46.0f;
    float rightLimit = max.x - 8.0f;
    if (row.fresh) {
        const char* tag = Label("NEW", "NEU", "NOUVEAU");
        const float tagW = TextWidth(big, 14.0f, tag);
        const ImVec2 tagMin(max.x - tagW - 18.0f, min.y + 9.0f);
        dl->AddRectFilled(tagMin, ImVec2(max.x - 8.0f, tagMin.y + 22.0f), WithAlpha(accent, 60), 10.0f);
        ShadowText(dl, big, 14.0f, ImVec2(tagMin.x + 5.0f, tagMin.y + 3.0f), accent, tag);
        rightLimit = tagMin.x - 6.0f;
    }
    if (row.state != State::Done && HintIndex::IsCheckHinted(row.rc)) {
        // A gossip stone (or another hint already read) named this check.
        const std::string tag = std::string(ICON_FA_STAR " ") + Label("Hint", "Hinweis", "Indice");
        const float tagW = TextWidth(big, 14.0f, tag.c_str());
        const ImVec2 tagMin(rightLimit - tagW - 10.0f, min.y + 9.0f);
        dl->AddRectFilled(tagMin, ImVec2(rightLimit, tagMin.y + 22.0f), IM_COL32(255, 215, 90, 50), 10.0f);
        ShadowText(dl, big, 14.0f, ImVec2(tagMin.x + 5.0f, tagMin.y + 3.0f), IM_COL32(255, 215, 90, 255), tag.c_str());
        rightLimit = tagMin.x - 6.0f;
    }
    ShadowText(dl, big, 18.0f, ImVec2(nameX, min.y + 9.0f), nameColor, label.c_str(), rightLimit);
    if (strike) {
        const float w = std::min(TextWidth(big, 18.0f, label.c_str()), rightLimit - nameX);
        dl->AddLine(ImVec2(nameX, min.y + 20.0f), ImVec2(nameX + w, min.y + 20.0f), WithAlpha(nameColor, 200), 1.5f);
    }
    if (!itemText.empty()) {
        const ImU32 itemColor = row.state == State::Done ? WithAlpha(kColDone, 200) : kColSeen;
        const std::string line = (row.state == State::Done ? std::string(ICON_FA_GIFT " ") : std::string(ICON_FA_EYE " ")) +
                                 itemText;
        ShadowText(dl, big, 16.0f, ImVec2(nameX, min.y + 33.0f), itemColor, line.c_str(), max.x - 8.0f);
    }

    if (expanded) {
        // Full location name and the skip / restore button.
        // Full name: area + check in French, the native long name otherwise.
        std::string fullName;
        const AreaDef* areaDef = AreaDefFor(sDetailArea);
        if (IsFrench() && areaDef != nullptr) {
            fullName = AreaName(*areaDef) + " : " + label;
        } else {
            fullName = Rando::StaticData::GetLocation(row.rc)->GetName();
        }
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.8f, 1.0f));
        ImGui::Indent(10.0f);
        ImGui::PushFont(big);
        ImGui::TextWrapped("%s", fullName.c_str());
        if (row.state == State::OtherAge) {
            // Why it is lavender: the logic needs the other age.
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kColOtherAge));
            ImGui::TextWrapped("%s %s", ICON_FA_HOURGLASS_HALF,
                               IsAdultNow() ? Label("Needs you to be a child.", "Nur als Kind.", "Faisable en enfant.")
                                            : Label("Needs you to be an adult.", "Nur als Erwachsener.",
                                                    "Faisable en adulte."));
            ImGui::PopStyleColor();
        }
        ImGui::PopFont();
        ImGui::Unindent(10.0f);
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        if (row.state != State::Done && row.rc != RC_LINKS_POCKET) {
            const bool skipped = row.state == State::Skipped;
            const char* text = skipped ? Label("Restore this check", "Wiederherstellen", "Ne plus ignorer")
                                       : Label("Skip this check", "Ignorieren", "Ignorer ce check");
            const ImVec2 bMin = ImGui::GetCursorScreenPos();
            const ImVec2 bSize(width, actionH - 8.0f);
            if (ImGui::InvisibleButton("##skip", bSize)) {
                CheckTracker::SetCheckSkippedByPlayer(row.rc, !skipped);
                sStatsDirty = true;
                sExpandedCheck = RC_UNKNOWN_CHECK;
            }
            DrawTextbox(dl, bMin, ImVec2(bMin.x + bSize.x, bMin.y + bSize.y), IM_COL32(20, 20, 60, 235));
            const std::string full = std::string(skipped ? ICON_FA_UNDO " " : ICON_FA_BAN " ") + text;
            const float tw = TextWidth(big, 18.0f, full.c_str());
            ShadowText(dl, big, 18.0f, ImVec2(bMin.x + (bSize.x - tw) * 0.5f, bMin.y + (bSize.y - 18.0f) * 0.5f - 1.0f),
                       IM_COL32(255, 255, 140, 255), full.c_str());
        }
    }
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.0f, 1.0f));
}

void DrawDetail() {
    const AreaDef* def = AreaDefFor(sDetailArea);
    if (def == nullptr) {
        sDetailArea = RCAREA_INVALID;
        DrawOverview();
        return;
    }
    const int regionIndex = RegionOf(sDetailArea);
    const RegionDef& region = Regions()[regionIndex];
    const AreaStats& stats = StatsFor(sDetailArea);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* huge = HugeFont();
    ImFont* big = BigFont();
    const float width = ImGui::GetContentRegionAvail().x;

    // Header: back button + area name in its region colour.
    {
        const float h = 46.0f;
        const ImVec2 min = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##back", ImVec2(h + 10.0f, h))) {
            OpenOverview(regionIndex);
        }
        DrawTextbox(dl, min, ImVec2(min.x + h + 10.0f, min.y + h), IM_COL32(20, 20, 60, 235));
        const float arrowW = TextWidth(huge, 22.0f, ICON_FA_CHEVRON_LEFT);
        ShadowText(dl, huge, 22.0f, ImVec2(min.x + (h + 10.0f - arrowW) * 0.5f, min.y + (h - 22.0f) * 0.5f),
                   IM_COL32(255, 255, 140, 255), ICON_FA_CHEVRON_LEFT);
        ImGui::SameLine(0.0f, 8.0f);
        const ImVec2 tMin = ImGui::GetCursorScreenPos();
        const ImVec2 tMax(min.x + width, min.y + h);
        ImGui::Dummy(ImVec2(tMax.x - tMin.x, h));
        dl->AddRectFilledMultiColor(tMin, tMax, WithAlpha(region.color, 110), WithAlpha(region.color, 20),
                                    WithAlpha(region.color, 20), WithAlpha(region.color, 110));
        DrawMedallion(dl, region, ImVec2(tMin.x + 6.0f, tMin.y + 8.0f), 30.0f);
        std::string name = AreaName(*def);
        if (CheckTracker::GetCurrentTrackerArea() == sDetailArea) {
            name = std::string(ICON_FA_MAP_MARKER " ") + name;
        }
        char counts[24];
        std::snprintf(counts, sizeof(counts), "%d/%d", stats.done, stats.total);
        const float countsW = TextWidth(big, 17.0f, counts);
        ShadowText(dl, big, 17.0f, ImVec2(tMax.x - countsW - 8.0f, tMin.y + 14.0f), kColSubText, counts);
        ShadowText(dl, huge, 22.0f, ImVec2(tMin.x + 44.0f, tMin.y + 11.0f), kColText, name.c_str(),
                   tMax.x - countsW - 16.0f);
    }

    if (!stats.spoiled) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::PushFont(big);
        ImGui::TextWrapped("%s", Label("Enter this dungeon (or find its map) to reveal its checks.",
                                       "Betritt dieses Dungeon, um seine Checks zu sehen.",
                                       "Entre dans ce donjon (ou trouve sa carte) pour voir ses checks."));
        ImGui::PopFont();
        return;
    }

    // Rows, most useful first: new, to do, other age, locked, skipped, done.
    std::vector<Row> rows;
    for (RandomizerCheck rc : CheckTracker::GetChecksForArea(sDetailArea)) {
        if (rc != RC_LINKS_POCKET && !CheckTracker::IsVisibleInCheckTracker(rc)) {
            continue;
        }
        const State state = GetState(rc);
        rows.push_back({ rc, state, (state == State::Todo || state == State::OtherAge) && IsFresh(rc) });
    }
    std::stable_sort(rows.begin(), rows.end(),
                     [](const Row& a, const Row& b) { return StateRank(a) < StateRank(b); });

    if (sDetailJustOpened) {
        ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
        sDetailJustOpened = false;
    }
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::BeginChild("##ChecksDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    const float rowW = ImGui::GetContentRegionAvail().x;
    for (const Row& row : rows) {
        DrawCheckRow(row, rowW);
    }
    if (rows.empty()) {
        ImGui::PushFont(big);
        ImGui::TextWrapped("%s", Label("Nothing to check here with this seed's settings.",
                                       "Hier gibt es mit diesen Einstellungen nichts.",
                                       "Rien à fouiller ici avec les règles de cette seed."));
        ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::EndChild();
}

void Draw(bool ready) {
    if (!ready) {
        ImGui::PushFont(BigFont());
        ImGui::TextWrapped("%s", Label("Waiting for a save to load...", "Warte auf einen Spielstand...",
                                       "En attente du chargement d'une partie..."));
        ImGui::PopFont();
        return;
    }
    DrawHeader();
    if (sDetailArea == RCAREA_INVALID) {
        DrawOverview();
    } else {
        DrawDetail();
    }
}

} // namespace Checks
