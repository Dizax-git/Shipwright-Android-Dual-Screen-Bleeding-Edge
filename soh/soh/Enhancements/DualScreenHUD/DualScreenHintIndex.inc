// What the player knows from the hints they have READ, sorted by tracker area
// (Thor build). Included inside the anonymous namespace of
// DualScreenTracker.cpp, before the Checks and Hint map tabs that use it.
//
// Nothing here may spoil the seed: only hints already read are looked at, and
// an "item somewhere in this area" hint never points at its exact check.

namespace HintIndex {

struct ItemRef {
    RandomizerHint key = RH_NONE;
    RandomizerCheck rc = RC_UNKNOWN_CHECK;
    std::string item;
    // True when the hint names the exact check (the stone told where it is);
    // false for "<item> is somewhere in <area>" hints.
    bool exact = false;
    bool obtained = false;
};

struct ZoneInfo {
    bool woth = false;
    bool foolish = false;
    std::vector<ItemRef> items;
    int stones = 0;
    int stonesRead = 0;

    bool HasKnowledge() const {
        return woth || foolish || !items.empty();
    }
    int PendingItems() const {
        int count = 0;
        for (const ItemRef& item : items) {
            if (!item.obtained) {
                count++;
            }
        }
        return count;
    }
};

std::unordered_map<int, ZoneInfo> sZones;
std::unordered_map<int, RandomizerHint> sHintedChecks;
int sReadCount = 0;
int sFrameCounter = 0;
bool sReady = false;
bool sDirty = true;

// Hint areas are a bit finer than the tracker areas (the Temple of Time is in
// the Market, Outside Ganon's Castle in Hyrule Castle...).
RandomizerCheckArea ZoneOf(RandomizerArea area) {
    switch (area) {
        case RA_KOKIRI_FOREST:
            return RCAREA_KOKIRI_FOREST;
        case RA_THE_LOST_WOODS:
            return RCAREA_LOST_WOODS;
        case RA_SACRED_FOREST_MEADOW:
            return RCAREA_SACRED_FOREST_MEADOW;
        case RA_HYRULE_FIELD:
            return RCAREA_HYRULE_FIELD;
        case RA_LAKE_HYLIA:
            return RCAREA_LAKE_HYLIA;
        case RA_GERUDO_VALLEY:
            return RCAREA_GERUDO_VALLEY;
        case RA_GERUDO_FORTRESS:
        case RA_THIEVES_HIDEOUT:
            return RCAREA_GERUDO_FORTRESS;
        case RA_HAUNTED_WASTELAND:
            return RCAREA_WASTELAND;
        case RA_DESERT_COLOSSUS:
            return RCAREA_DESERT_COLOSSUS;
        case RA_THE_MARKET:
        case RA_TEMPLE_OF_TIME:
            return RCAREA_MARKET;
        case RA_HYRULE_CASTLE:
        case RA_OUTSIDE_GANONS_CASTLE:
        case RA_CASTLE_GROUNDS:
            return RCAREA_HYRULE_CASTLE;
        case RA_KAKARIKO_VILLAGE:
            return RCAREA_KAKARIKO_VILLAGE;
        case RA_THE_GRAVEYARD:
            return RCAREA_GRAVEYARD;
        case RA_DEATH_MOUNTAIN_TRAIL:
            return RCAREA_DEATH_MOUNTAIN_TRAIL;
        case RA_GORON_CITY:
            return RCAREA_GORON_CITY;
        case RA_DEATH_MOUNTAIN_CRATER:
            return RCAREA_DEATH_MOUNTAIN_CRATER;
        case RA_ZORAS_RIVER:
            return RCAREA_ZORAS_RIVER;
        case RA_ZORAS_DOMAIN:
            return RCAREA_ZORAS_DOMAIN;
        case RA_ZORAS_FOUNTAIN:
            return RCAREA_ZORAS_FOUNTAIN;
        case RA_LON_LON_RANCH:
            return RCAREA_LON_LON_RANCH;
        case RA_DEKU_TREE:
            return RCAREA_DEKU_TREE;
        case RA_DODONGOS_CAVERN:
            return RCAREA_DODONGOS_CAVERN;
        case RA_JABU_JABUS_BELLY:
            return RCAREA_JABU_JABUS_BELLY;
        case RA_FOREST_TEMPLE:
            return RCAREA_FOREST_TEMPLE;
        case RA_FIRE_TEMPLE:
            return RCAREA_FIRE_TEMPLE;
        case RA_WATER_TEMPLE:
            return RCAREA_WATER_TEMPLE;
        case RA_SPIRIT_TEMPLE:
            return RCAREA_SPIRIT_TEMPLE;
        case RA_SHADOW_TEMPLE:
            return RCAREA_SHADOW_TEMPLE;
        case RA_BOTTOM_OF_THE_WELL:
            return RCAREA_BOTTOM_OF_THE_WELL;
        case RA_ICE_CAVERN:
            return RCAREA_ICE_CAVERN;
        case RA_GERUDO_TRAINING_GROUND:
            return RCAREA_GERUDO_TRAINING_GROUND;
        case RA_GANONS_CASTLE:
            return RCAREA_GANONS_CASTLE;
        default:
            return RCAREA_INVALID;
    }
}

std::string Capitalized(std::string text) {
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') {
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    }
    return text;
}

void Reset() {
    sZones.clear();
    sHintedChecks.clear();
    sReadCount = 0;
    sReady = false;
    sDirty = true;
}

void AddItem(RandomizerCheckArea zone, const ItemRef& ref) {
    if (zone == RCAREA_INVALID) {
        return;
    }
    std::vector<ItemRef>& items = sZones[zone].items;
    for (const ItemRef& other : items) {
        // Several stones often repeat the same hint: list it once.
        if (other.rc == ref.rc && other.exact == ref.exact) {
            return;
        }
    }
    items.push_back(ref);
}

void Rebuild() {
    sZones.clear();
    sHintedChecks.clear();
    sReadCount = 0;
    auto ctx = Rando::Context::GetInstance();
    const bool mysterious = CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0) != 0;

    // Gossip stones of each area: how many there are and how many were read.
    // Their number is not a spoiler; their content stays hidden until read.
    for (auto& [stone, hintKey] : Rando::StaticData::gossipStoneCheckToHint) {
        if (!ctx->GetHint(hintKey)->IsEnabled()) {
            continue;
        }
        const RandomizerCheckArea zone = Rando::StaticData::GetLocation(stone)->GetArea();
        if (zone == RCAREA_INVALID) {
            continue;
        }
        ZoneInfo& info = sZones[zone];
        info.stones++;
        if (HintTracker::IsHintReadByPlayer(hintKey)) {
            info.stonesRead++;
        }
    }

    for (int i = RH_NONE + 1; i < RH_MAX; i++) {
        const RandomizerHint key = static_cast<RandomizerHint>(i);
        Rando::Hint* hint = ctx->GetHint(key);
        if (hint == nullptr || !hint->IsEnabled() || !HintTracker::IsHintReadByPlayer(key)) {
            continue;
        }
        sReadCount++;
        const HintType type = hint->GetHintType();
        if (type == HINT_TYPE_WOTH || type == HINT_TYPE_FOOLISH) {
            for (RandomizerArea area : hint->GetHintedAreas()) {
                const RandomizerCheckArea zone = ZoneOf(area);
                if (zone == RCAREA_INVALID) {
                    continue;
                }
                if (type == HINT_TYPE_WOTH) {
                    sZones[zone].woth = true;
                } else {
                    sZones[zone].foolish = true;
                }
            }
        } else if (type == HINT_TYPE_ITEM) {
            // The hint names the place: point at the exact check.
            const std::vector<RandomizerCheck> locations = hint->GetHintedLocations();
            for (size_t slot = 0; slot < locations.size() && slot < 255; slot++) {
                const RandomizerCheck rc = locations[slot];
                ItemRef ref;
                ref.key = key;
                ref.rc = rc;
                ref.exact = true;
                ref.item = Capitalized(
                    hint->GetItemName(static_cast<uint8_t>(slot), mysterious).GetForCurrentLanguage(MF_CLEAN));
                ref.obtained = ctx->GetItemLocation(rc)->HasObtained();
                AddItem(Rando::StaticData::GetLocation(rc)->GetArea(), ref);
                sHintedChecks[static_cast<int>(rc)] = key;
            }
        } else if (type == HINT_TYPE_ITEM_AREA || type == HINT_TYPE_AREA) {
            // "<item> is somewhere in <area>": attach it to the area only.
            const std::vector<RandomizerCheck> locations = hint->GetHintedLocations();
            const std::vector<RandomizerArea> areas = hint->GetHintedAreas();
            for (size_t slot = 0; slot < locations.size() && slot < areas.size() && slot < 255; slot++) {
                const RandomizerCheck rc = locations[slot];
                ItemRef ref;
                ref.key = key;
                ref.rc = rc;
                ref.exact = false;
                ref.item = Capitalized(
                    hint->GetItemName(static_cast<uint8_t>(slot), mysterious).GetForCurrentLanguage(MF_CLEAN));
                ref.obtained = ctx->GetItemLocation(rc)->HasObtained();
                AddItem(ZoneOf(areas[slot]), ref);
            }
        }
    }
}

// Called every frame from DrawTrackerUi, whatever the tab.
bool Update() {
    if (!GameInteractor::IsSaveLoaded() || !IS_RANDO) {
        if (sReady || !sZones.empty()) {
            Reset();
        }
        return false;
    }
    if (sDirty || !sReady || ++sFrameCounter >= 30) {
        sFrameCounter = 0;
        sDirty = false;
        Rebuild();
        sReady = true;
    }
    return true;
}

const ZoneInfo& Get(RandomizerCheckArea zone) {
    static const ZoneInfo empty;
    auto it = sZones.find(zone);
    return it == sZones.end() ? empty : it->second;
}

bool IsCheckHinted(RandomizerCheck rc) {
    return sHintedChecks.contains(static_cast<int>(rc));
}

int ReadCount() {
    return sReadCount;
}

} // namespace HintIndex
