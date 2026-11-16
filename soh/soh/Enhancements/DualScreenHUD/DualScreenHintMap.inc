// Bottom-screen "Hints" tab (Thor build): the pause menu's world map of Hyrule.
// Included inside the anonymous namespace of DualScreenTracker.cpp, after the
// Checks tab (it reuses its drawing helpers and area names).
//
// Each place of the map gets a marker summing up what the hints the player has
// READ say about it (never more, nothing is spoiled):
//   blue       = Way of the Hero
//   pink, X    = Foolish (nothing useful there)
//   gold star  = an item was announced there (count of those not found yet)
//   small grey = a gossip stone of this place is still unread
// The current area is lit like on the pause map (cyan area box). Tapping a
// place opens a dialogue box with what is known; entering an area opens it.
// The original journal stays one tap away ("Journal" button).

namespace HintMap {

constexpr ImU32 kColWoth = IM_COL32(100, 180, 255, 255);
constexpr ImU32 kColFoolish = IM_COL32(255, 150, 180, 255);
constexpr ImU32 kColStar = IM_COL32(255, 215, 90, 255);
constexpr ImU32 kColNeutral = IM_COL32(255, 255, 210, 170);
constexpr ImU32 kColStone = IM_COL32(190, 190, 205, 255);

// Pause map world texture (icon_item_field_static/gWorldMapImageTex).
constexpr float kMapW = 216.0f;
constexpr float kMapH = 128.0f;

// Pause map area boxes (z_kaleido_map_PAL.c), in the map's own pixels:
// x = areaBoxPosX + 108, y = 58 - areaBoxPosY.
struct AreaBox {
    int posX;
    int posY;
    int w;
    int h;
    int tex; // gWorldMapAreaBox<tex>Tex
};
const AreaBox kBoxes[22] = {
    { -41, 30, 96, 59, 7 },  { 19, 36, 32, 19, 1 },  { 44, 35, 32, 13, 4 },  { 40, 26, 48, 19, 6 },
    { 49, 7, 48, 38, 2 },    { 51, 11, 32, 17, 3 },  { -49, -31, 48, 38, 2 }, { 83, 30, 32, 17, 3 },
    { 80, 38, 32, 13, 4 },   { -67, 23, 32, 26, 5 }, { 50, 2, 16, 16, 8 },    { -109, 42, 32, 26, 5 },
    { -76, 40, 32, 26, 5 },  { -86, 32, 16, 16, 8 }, { -10, 38, 32, 19, 1 },  { -6, 50, 32, 17, 3 },
    { 19, 57, 32, 26, 5 },   { 24, 58, 32, 13, 4 },  { 11, 56, 32, 17, 3 },   { -17, 12, 32, 17, 3 },
    { 37, 36, 16, 16, 8 },   { -6, 50, 32, 17, 3 },
};

// One marker per place of the map; dungeons share their entrance's marker.
struct Spot {
    float x; // map pixels
    float y;
    int box; // pause map area box lit when Link is here
    std::vector<RandomizerCheckArea> areas;
};

const std::vector<Spot>& Spots() {
    static const std::vector<Spot> spots = {
        { 183.0f, 74.0f, 4, { RCAREA_KOKIRI_FOREST, RCAREA_DEKU_TREE } },
        { 166.0f, 64.0f, 10, { RCAREA_LOST_WOODS } },
        { 175.0f, 52.0f, 5, { RCAREA_SACRED_FOREST_MEADOW, RCAREA_FOREST_TEMPLE } },
        { 128.0f, 70.0f, 0, { RCAREA_HYRULE_FIELD } },
        { 105.0f, 54.0f, 19, { RCAREA_LON_LON_RANCH } },
        { 114.0f, 30.0f, 14, { RCAREA_MARKET } },
        { 118.0f, 15.0f, 15, { RCAREA_HYRULE_CASTLE, RCAREA_GANONS_CASTLE } },
        { 143.0f, 33.0f, 1, { RCAREA_KAKARIKO_VILLAGE, RCAREA_BOTTOM_OF_THE_WELL } },
        { 165.0f, 27.0f, 2, { RCAREA_GRAVEYARD, RCAREA_SHADOW_TEMPLE } },
        { 146.0f, 19.0f, 16, { RCAREA_DEATH_MOUNTAIN_TRAIL, RCAREA_DODONGOS_CAVERN } },
        { 129.0f, 7.0f, 18, { RCAREA_GORON_CITY } },
        { 154.0f, 5.0f, 17, { RCAREA_DEATH_MOUNTAIN_CRATER, RCAREA_FIRE_TEMPLE } },
        { 178.0f, 40.0f, 3, { RCAREA_ZORAS_RIVER } },
        { 209.0f, 41.0f, 7, { RCAREA_ZORAS_DOMAIN } },
        { 203.0f, 23.0f, 8, { RCAREA_ZORAS_FOUNTAIN, RCAREA_JABU_JABUS_BELLY, RCAREA_ICE_CAVERN } },
        { 83.0f, 106.0f, 6, { RCAREA_LAKE_HYLIA, RCAREA_WATER_TEMPLE } },
        { 57.0f, 48.0f, 9, { RCAREA_GERUDO_VALLEY } },
        { 48.0f, 30.0f, 12, { RCAREA_GERUDO_FORTRESS, RCAREA_GERUDO_TRAINING_GROUND } },
        { 30.0f, 41.0f, 13, { RCAREA_WASTELAND } },
        { 13.0f, 25.0f, 11, { RCAREA_DESERT_COLOSSUS, RCAREA_SPIRIT_TEMPLE } },
    };
    return spots;
}

int SpotOf(RandomizerCheckArea area) {
    const auto& spots = Spots();
    for (size_t i = 0; i < spots.size(); i++) {
        for (RandomizerCheckArea a : spots[i].areas) {
            if (a == area) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

std::string ZoneName(RandomizerCheckArea area) {
    const Checks::AreaDef* def = Checks::AreaDefFor(area);
    return def != nullptr ? Checks::AreaName(*def) : RandomizerCheckObjects::GetRCAreaName(area);
}

int sSelectedSpot = -1;
bool sPanelJustOpened = false;
RandomizerCheckArea sFollowedArea = RCAREA_INVALID;
int sSeenReadCount = -1;
bool sTabVisible = false;

// Summary of a place, all its areas together.
struct SpotSummary {
    bool woth = false;
    bool foolish = false;
    int pending = 0;
    int unreadStones = 0;
    bool known = false;
};

SpotSummary Summarize(const Spot& spot) {
    SpotSummary s;
    for (RandomizerCheckArea area : spot.areas) {
        const HintIndex::ZoneInfo& info = HintIndex::Get(area);
        s.woth = s.woth || info.woth;
        // A place is only shown as foolish when its main area is.
        if (area == spot.areas.front()) {
            s.foolish = info.foolish;
        }
        s.pending += info.PendingItems();
        s.unreadStones += info.stones - info.stonesRead;
        s.known = s.known || info.HasKnowledge();
    }
    return s;
}

// Called every frame from DrawTrackerUi, whatever the tab: follows Link.
void Update(bool ready) {
    if (!ready) {
        sSelectedSpot = -1;
        sFollowedArea = RCAREA_INVALID;
        sSeenReadCount = -1;
        return;
    }
    if (sSeenReadCount < 0) {
        // Loading a save is not news: start from what was already read.
        sSeenReadCount = HintIndex::ReadCount();
    }
    const RandomizerCheckArea current = CheckTracker::GetCurrentTrackerArea();
    if (current != sFollowedArea) {
        const int before = SpotOf(sFollowedArea);
        sFollowedArea = current;
        const int now = SpotOf(current);
        if (CVarGetInteger(CVAR_ENHANCEMENT("DualScreenChecksFollow"), 1) && now >= 0 && now != before) {
            sSelectedSpot = now;
            sPanelJustOpened = true;
        }
    }
    if (sTabVisible) {
        sSeenReadCount = HintIndex::ReadCount();
    }
    sTabVisible = false;
}

// A new hint was read since the tab was last looked at.
bool HasNews() {
    return sSeenReadCount >= 0 && HintIndex::ReadCount() > sSeenReadCount;
}

ImVec2 MapToScreen(ImVec2 mapMin, float scale, float x, float y) {
    return ImVec2(mapMin.x + x * scale, mapMin.y + y * scale);
}

void DrawMarker(ImDrawList* dl, ImVec2 c, const SpotSummary& s, bool here) {
    ImFont* big = BigFont();
    const float r = 8.0f;
    ImU32 color = s.known ? kColStar : kColNeutral;
    if (s.foolish) {
        color = kColFoolish;
    } else if (s.woth) {
        color = kColWoth;
    }
    if (s.woth && !s.foolish) {
        // Way of the Hero: a slow pulsing halo.
        dl->AddCircleFilled(c, r + 5.0f + 2.0f * Checks::Pulse(3.0f), Checks::WithAlpha(kColWoth, 70));
    }
    dl->AddCircleFilled(ImVec2(c.x + 1.5f, c.y + 1.5f), r, IM_COL32(0, 0, 0, 170));
    ImTextureID dot = Tex("DST_WM_DOT");
    if (dot != nullptr) {
        dl->AddImage(dot, ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), ImVec2(0, 0), ImVec2(1, 1), color);
        dl->AddCircle(c, r - 1.0f, Checks::WithAlpha(color, 255), 0, 1.5f);
    } else {
        dl->AddCircleFilled(c, r, color);
    }
    if (s.foolish) {
        const float k = r * 0.55f;
        dl->AddLine(ImVec2(c.x - k, c.y - k), ImVec2(c.x + k, c.y + k), IM_COL32(60, 10, 30, 255), 2.5f);
        dl->AddLine(ImVec2(c.x - k, c.y + k), ImVec2(c.x + k, c.y - k), IM_COL32(60, 10, 30, 255), 2.5f);
    }
    if (s.pending > 0) {
        char text[16];
        std::snprintf(text, sizeof(text), ICON_FA_STAR "%d", s.pending);
        Checks::ShadowText(dl, big, 14.0f, ImVec2(c.x + r - 1.0f, c.y - r - 11.0f), kColStar, text);
    }
    if (s.unreadStones > 0) {
        dl->AddCircleFilled(ImVec2(c.x - r - 2.0f, c.y + r + 1.0f), 3.5f, IM_COL32(0, 0, 0, 200));
        dl->AddCircleFilled(ImVec2(c.x - r - 2.5f, c.y + r + 0.5f), 3.0f, kColStone);
    }
    if (here) {
        // Where Link is: the map marker icon above the dot.
        const char* pin = ICON_FA_MAP_MARKER;
        const float w = Checks::TextWidth(big, 18.0f, pin);
        const float bob = 2.0f * Checks::Pulse(4.0f);
        Checks::ShadowText(dl, big, 18.0f, ImVec2(c.x - w * 0.5f, c.y - r - 22.0f - bob), IM_COL32(255, 80, 60, 255),
                           pin);
    }
}

void DrawSmallButton(const char* id, const std::string& text, ImVec2 min, ImVec2 size, bool* pressed) {
    ImGui::SetCursorScreenPos(min);
    *pressed = ImGui::InvisibleButton(id, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    DrawTextbox(dl, min, max, IM_COL32(20, 20, 60, 235));
    ImFont* big = BigFont();
    const float tw = Checks::TextWidth(big, 17.0f, text.c_str());
    Checks::ShadowText(dl, big, 17.0f, ImVec2(min.x + (size.x - tw) * 0.5f, min.y + (size.y - 17.0f) * 0.5f - 1.0f),
                       IM_COL32(255, 255, 140, 255), text.c_str());
}

// Opens the Checks tab on this check.
void JumpToCheck(RandomizerCheckArea area, RandomizerCheck rc) {
    if (Checks::AreaDefFor(area) == nullptr) {
        return;
    }
    Checks::sDetailArea = area;
    Checks::sDetailJustOpened = true;
    Checks::sExpandedCheck = rc;
    CVarSetInteger(CVAR_ENHANCEMENT("DualScreenTrackerTab"), TAB_CHECKS);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

// The dialogue box listing what is known about one place.
void DrawPanel(const Spot& spot, ImVec2 min, ImVec2 size) {
    ImDrawList* parentDl = ImGui::GetWindowDrawList();
    DrawTextbox(parentDl, min, ImVec2(min.x + size.x, min.y + size.y), IM_COL32(0, 0, 0, 225));

    ImGui::SetCursorScreenPos(min);
    if (sPanelJustOpened) {
        ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
        sPanelJustOpened = false;
    }
    ImGui::BeginChild("##HintMapPanel", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* big = BigFont();
    ImFont* huge = HugeFont();
    const float pad = 14.0f;
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // Title + close button.
    {
        const float h = 40.0f;
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const RandomizerCheckArea main = spot.areas.front();
        std::string title = ZoneName(main);
        if (SpotOf(CheckTracker::GetCurrentTrackerArea()) == static_cast<int>(&spot - &Spots()[0])) {
            title = std::string(ICON_FA_MAP_MARKER " ") + title;
        }
        Checks::ShadowText(dl, huge, 22.0f, ImVec2(rowMin.x + pad, rowMin.y + 8.0f), IM_COL32(255, 255, 140, 255),
                           title.c_str(), rowMin.x + width - h - 12.0f);
        bool close = false;
        DrawSmallButton("##close", ICON_FA_TIMES, ImVec2(rowMin.x + width - h - 6.0f, rowMin.y), ImVec2(h, h), &close);
        if (close) {
            sSelectedSpot = -1;
        }
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + h + 4.0f));
    }

    bool anything = false;
    for (RandomizerCheckArea area : spot.areas) {
        const HintIndex::ZoneInfo& info = HintIndex::Get(area);
        const bool isMain = area == spot.areas.front();
        if (!isMain && !info.HasKnowledge() && info.stones == 0) {
            continue;
        }
        if (!isMain) {
            // Dungeon of this place: its own small heading.
            const ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(p.x + pad, p.y + 2.0f), ImVec2(p.x + width - pad, p.y + 2.0f), IM_COL32(255, 255, 140, 70),
                        1.0f);
            Checks::ShadowText(dl, huge, 19.0f, ImVec2(p.x + pad, p.y + 8.0f), IM_COL32(240, 220, 150, 255),
                               ZoneName(area).c_str(), p.x + width - pad);
            ImGui::Dummy(ImVec2(width, 34.0f));
        }

        // Way of the Hero / Foolish chips.
        if (info.woth || info.foolish) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            float x = p.x + pad;
            auto chip = [&](const char* text, ImU32 color) {
                const float tw = Checks::TextWidth(big, 17.0f, text);
                dl->AddRectFilled(ImVec2(x, p.y), ImVec2(x + tw + 20.0f, p.y + 30.0f), Checks::WithAlpha(color, 55),
                                  15.0f);
                dl->AddRect(ImVec2(x, p.y), ImVec2(x + tw + 20.0f, p.y + 30.0f), Checks::WithAlpha(color, 200), 15.0f,
                            0, 1.5f);
                Checks::ShadowText(dl, big, 17.0f, ImVec2(x + 10.0f, p.y + 5.0f), color, text);
                x += tw + 30.0f;
            };
            if (info.woth) {
                chip(Label("Way of the Hero", "Weg des Helden", "Voie du héros"), kColWoth);
            }
            if (info.foolish) {
                chip(Label("Foolish", "Unnütz", "Inutile"), kColFoolish);
            }
            ImGui::Dummy(ImVec2(width, 36.0f));
            anything = true;
        }

        // Announced items: the exact check when the hint named it.
        for (const HintIndex::ItemRef& item : info.items) {
            anything = true;
            const float h = 52.0f;
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushID(static_cast<int>(item.rc) * 2 + (item.exact ? 1 : 0));
            const bool tapped = ImGui::InvisibleButton("##item", ImVec2(width, h));
            ImGui::PopID();
            if (tapped && item.exact) {
                JumpToCheck(area, item.rc);
            }
            const ImU32 nameColor = item.obtained ? Checks::kColDoneText : kColStar;
            const char* icon = item.obtained ? ICON_FA_CHECK : ICON_FA_STAR;
            Checks::ShadowText(dl, big, 18.0f, ImVec2(p.x + pad, p.y + 5.0f),
                               item.obtained ? Checks::kColDone : kColStar, icon);
            const float textX = p.x + pad + 28.0f;
            const float right = p.x + width - pad - (item.exact ? 22.0f : 0.0f);
            Checks::ShadowText(dl, big, 18.0f, ImVec2(textX, p.y + 4.0f), nameColor, item.item.c_str(), right);
            if (item.obtained) {
                const float w = std::min(Checks::TextWidth(big, 18.0f, item.item.c_str()), right - textX);
                dl->AddLine(ImVec2(textX, p.y + 15.0f), ImVec2(textX + w, p.y + 15.0f),
                            Checks::WithAlpha(nameColor, 200), 1.5f);
            }
            std::string where;
            ImU32 whereColor = Checks::kColSubText;
            if (item.exact) {
                const Checks::State state = Checks::GetState(item.rc);
                const char* stateIcon = ICON_FA_CIRCLE_O;
                switch (state) {
                    case Checks::State::Todo:
                        stateIcon = ICON_FA_ARROW_CIRCLE_RIGHT;
                        whereColor = Checks::kColTodo;
                        break;
                    case Checks::State::Locked:
                        stateIcon = ICON_FA_LOCK;
                        whereColor = Checks::kColLocked;
                        break;
                    case Checks::State::Done:
                        stateIcon = ICON_FA_CHECK;
                        whereColor = Checks::kColDone;
                        break;
                    case Checks::State::Skipped:
                        stateIcon = ICON_FA_BAN;
                        whereColor = Checks::kColLocked;
                        break;
                    default:
                        break;
                }
                where = std::string(stateIcon) + " " + CheckTracker::GetCheckLabel(item.rc);
            } else {
                where = item.obtained ? std::string(Label("Found", "Gefunden", "Trouvé"))
                                      : std::string(Label("Somewhere here", "Irgendwo hier", "Quelque part ici"));
            }
            Checks::ShadowText(dl, big, 15.0f, ImVec2(textX, p.y + 28.0f), whereColor, where.c_str(), right);
            if (item.exact) {
                Checks::ShadowText(dl, big, 16.0f, ImVec2(p.x + width - pad - 14.0f, p.y + 17.0f),
                                   IM_COL32(255, 255, 140, 200), ICON_FA_CHEVRON_RIGHT);
            }
        }

        // Gossip stones of this area.
        if (info.stones > 0) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const int unread = info.stones - info.stonesRead;
            char text[96];
            if (unread > 0) {
                std::snprintf(text, sizeof(text), "%s %d %s %d  ·  %d %s", ICON_FA_COMMENT,
                              info.stonesRead, Label("read of", "von", "lue(s) sur"), info.stones, unread,
                              Label("to read", "offen", "à lire"));
            } else {
                std::snprintf(text, sizeof(text), "%s %s (%d)", ICON_FA_COMMENT,
                              Label("All gossip stones read", "Alle Steine gelesen", "Pierres à potins toutes lues"),
                              info.stones);
            }
            Checks::ShadowText(dl, big, 16.0f, ImVec2(p.x + pad, p.y + 4.0f), unread > 0 ? kColStone : Checks::kColDoneText,
                               text, p.x + width - pad);
            ImGui::Dummy(ImVec2(width, 28.0f));
        }
    }

    if (!anything) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(p.x + pad, p.y + 2.0f));
        ImGui::PushTextWrapPos(p.x + width - pad);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.84f, 1.0f));
        ImGui::PushFont(big);
        ImGui::TextWrapped("%s", Label("No hint read about this place yet.",
                                       "Noch kein Hinweis zu diesem Ort gelesen.",
                                       "Aucun indice lu sur ce lieu pour l'instant."));
        ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    }
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::EndChild();
}

void DrawJournal() {
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 min = ImGui::GetCursorScreenPos();
    bool pressed = false;
    const std::string label = std::string(ICON_FA_MAP " ") + Label("Map", "Karte", "Carte");
    DrawSmallButton("##toMap", label, ImVec2(min.x + width - 140.0f, min.y), ImVec2(140.0f, 40.0f), &pressed);
    Checks::ShadowText(ImGui::GetWindowDrawList(), HugeFont(), 21.0f, ImVec2(min.x + 6.0f, min.y + 9.0f),
                       IM_COL32(255, 255, 140, 255), Label("Journal", "Tagebuch", "Journal"));
    if (pressed) {
        CVarSetInteger(CVAR_ENHANCEMENT("DualScreenHintsJournal"), 0);
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    ImGui::SetCursorScreenPos(ImVec2(min.x, min.y + 46.0f));
    ImGui::BeginChild("##DualScreenHints", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImGui::PushFont(BigFont());
    HintTracker::DrawEmbedded();
    ImGui::PopFont();
    ImGui::EndChild();
}

void Draw(bool ready) {
    sTabVisible = true;
    if (CVarGetInteger(CVAR_ENHANCEMENT("DualScreenHintsJournal"), 0)) {
        DrawJournal();
        return;
    }
    if (!ready) {
        ImGui::PushFont(BigFont());
        ImGui::TextWrapped("%s", Label("Waiting for a randomizer save to load...", "Warte auf einen Spielstand...",
                                       "En attente du chargement d'une partie randomizer..."));
        ImGui::PopFont();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* big = BigFont();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float headerH = 40.0f;
    const float legendH = 24.0f;
    const auto& spots = Spots();
    const RandomizerCheckArea currentArea = CheckTracker::GetCurrentTrackerArea();
    const int hereSpot = SpotOf(currentArea);

    // Header: where Link is, and the way to the full journal.
    {
        std::string here;
        if (currentArea != RCAREA_INVALID) {
            here = std::string(ICON_FA_MAP_MARKER " ") + Label("Here: ", "Hier: ", "Ici : ") + ZoneName(currentArea);
        } else {
            char count[48];
            std::snprintf(count, sizeof(count), "%d %s", HintIndex::ReadCount(),
                          Label("hints read", "Hinweise gelesen", "indices lus"));
            here = count;
        }
        Checks::ShadowText(dl, HugeFont(), 20.0f, ImVec2(origin.x + 6.0f, origin.y + 10.0f), Checks::kColText,
                           here.c_str(), origin.x + avail.x - 150.0f);
        bool pressed = false;
        const std::string label = std::string(ICON_FA_BOOK " ") + Label("Journal", "Tagebuch", "Journal");
        DrawSmallButton("##toJournal", label, ImVec2(origin.x + avail.x - 140.0f, origin.y), ImVec2(140.0f, headerH),
                        &pressed);
        if (pressed) {
            CVarSetInteger(CVAR_ENHANCEMENT("DualScreenHintsJournal"), 1);
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
    }

    // The map, as large as the space allows, centred.
    const float roomH = std::max(60.0f, avail.y - headerH - legendH - 14.0f);
    float mapWidth = avail.x;
    float mapHeight = mapWidth * kMapH / kMapW;
    if (mapHeight > roomH) {
        mapHeight = roomH;
        mapWidth = mapHeight * kMapW / kMapH;
    }
    const float scale = mapWidth / kMapW;
    const ImVec2 mapMin(origin.x + (avail.x - mapWidth) * 0.5f, origin.y + headerH + 6.0f);
    const ImVec2 mapMax(mapMin.x + mapWidth, mapMin.y + mapHeight);

    dl->AddRectFilled(ImVec2(mapMin.x - 4.0f, mapMin.y - 4.0f), ImVec2(mapMax.x + 4.0f, mapMax.y + 4.0f),
                      IM_COL32(0, 0, 0, 220), 6.0f);
    ImTextureID world = Tex("DST_WORLD_MAP");
    if (world != nullptr) {
        dl->AddImage(world, mapMin, mapMax);
    } else {
        dl->AddRectFilled(mapMin, mapMax, IM_COL32(40, 70, 40, 255));
    }
    dl->AddRect(ImVec2(mapMin.x - 3.0f, mapMin.y - 3.0f), ImVec2(mapMax.x + 3.0f, mapMax.y + 3.0f),
                IM_COL32(210, 170, 70, 255), 6.0f, 0, 2.5f);

    // Current area lit like on the pause map.
    if (hereSpot >= 0) {
        const AreaBox& box = kBoxes[spots[hereSpot].box];
        const ImVec2 bMin = MapToScreen(mapMin, scale, static_cast<float>(box.posX + 108),
                                        static_cast<float>(58 - box.posY));
        const ImVec2 bMax(bMin.x + box.w * scale, bMin.y + box.h * scale);
        char name[16];
        std::snprintf(name, sizeof(name), "DST_WM_BOX%d", box.tex);
        ImTextureID boxTex = Tex(name);
        const int alpha = static_cast<int>(110 + 110 * Checks::Pulse(3.0f));
        if (boxTex != nullptr) {
            dl->AddImage(boxTex, bMin, bMax, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(100, 255, 255, alpha));
        } else {
            dl->AddRectFilled(bMin, bMax, IM_COL32(100, 255, 255, alpha / 3), 4.0f);
        }
    }

    // Markers (touch targets first, so a tap anywhere near a dot selects it).
    const bool panelOpen = sSelectedSpot >= 0 && sSelectedSpot < static_cast<int>(spots.size());
    const float panelH = mapHeight * 0.58f;
    const float panelTop = mapMax.y - panelH;
    for (size_t i = 0; i < spots.size(); i++) {
        const Spot& spot = spots[i];
        const ImVec2 c = MapToScreen(mapMin, scale, spot.x, spot.y);
        if (panelOpen && c.y > panelTop - 6.0f) {
            continue; // hidden under the dialogue box
        }
        const float hit = 20.0f;
        ImGui::SetCursorScreenPos(ImVec2(c.x - hit, c.y - hit));
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::InvisibleButton("##spot", ImVec2(hit * 2.0f, hit * 2.0f))) {
            sSelectedSpot = sSelectedSpot == static_cast<int>(i) ? -1 : static_cast<int>(i);
            sPanelJustOpened = true;
        }
        ImGui::PopID();
    }
    for (size_t i = 0; i < spots.size(); i++) {
        const Spot& spot = spots[i];
        const ImVec2 c = MapToScreen(mapMin, scale, spot.x, spot.y);
        if (panelOpen && c.y > panelTop - 6.0f) {
            continue;
        }
        DrawMarker(dl, c, Summarize(spot), static_cast<int>(i) == hereSpot);
        if (static_cast<int>(i) == sSelectedSpot) {
            DrawPauseCursor(dl, ImVec2(c.x - 12.0f, c.y - 12.0f), ImVec2(c.x + 12.0f, c.y + 12.0f));
        }
    }

    // Legend under the map.
    {
        const float y = mapMax.y + 9.0f;
        float x = origin.x + 6.0f;
        auto entry = [&](ImU32 color, const char* icon, const char* text) {
            if (icon != nullptr) {
                Checks::ShadowText(dl, big, 14.0f, ImVec2(x, y + 1.0f), color, icon);
                x += Checks::TextWidth(big, 14.0f, icon) + 5.0f;
            } else {
                dl->AddCircleFilled(ImVec2(x + 6.0f, y + 9.0f), 6.0f, color);
                x += 17.0f;
            }
            Checks::ShadowText(dl, big, 14.0f, ImVec2(x, y + 1.0f), Checks::kColSubText, text);
            x += Checks::TextWidth(big, 14.0f, text) + 16.0f;
        };
        entry(kColWoth, nullptr, Label("Hero", "Held", "Héros"));
        entry(kColFoolish, nullptr, Label("Foolish", "Unnütz", "Inutile"));
        entry(kColStar, ICON_FA_STAR, Label("Item told", "Gegenstand", "Objet annoncé"));
        entry(kColStone, ICON_FA_COMMENT, Label("Stone to read", "Stein offen", "Pierre à lire"));
    }

    if (panelOpen) {
        DrawPanel(spots[sSelectedSpot], ImVec2(mapMin.x, panelTop), ImVec2(mapWidth, panelH));
    }

    // Reserve the whole area so the window layout stays put.
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(avail.x, std::max(0.0f, avail.y - 2.0f)));
}

} // namespace HintMap
