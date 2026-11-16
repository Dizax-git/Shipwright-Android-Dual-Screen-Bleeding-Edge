#pragma once

#include <nlohmann/json.hpp>
#include "randomizerTypes.h"
#include "soh/SohGui/UIWidgets.hpp"

namespace CheckTracker {

class CheckTrackerSettingsWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    ~CheckTrackerSettingsWindow(){};

  protected:
    void InitElement() override{};
    void DrawElement() override;
    void UpdateElement() override{};
};

class CheckTrackerWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void Draw() override;
    ~CheckTrackerWindow(){};

  protected:
    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override;
};

// Converts an index into a Little Endian bitmask, as follows:
// 00: 0000000100000000
// 01: 0000001000000000
//...
// 06: 0100000000000000
// 07: 1000000000000000
// 08: 0000000000000001
// 09: 0000000000000010
//...
// 14: 0000000001000000
// 15: 0000000010000000
// repeat...
#define INDEX_TO_16BIT_LITTLE_ENDIAN_BITMASK(idx) (0x8000 >> (7 - (idx % 8) + ((idx % 16) / 8) * 8))

void Teardown();
void UpdateAllOrdering();
bool IsVisibleInCheckTracker(RandomizerCheck rc);
bool IsCheckShuffled(RandomizerCheck rc);
void InitTrackerData(bool isDebug);
RandomizerCheckArea GetCheckArea();
uint16_t GetTotalChecks();
uint16_t GetTotalChecksGotten();
bool IsAreaSpoiled(RandomizerCheckArea rcArea);
void SetAreaSpoiled(RandomizerCheckArea rcArea);
void UpdateInventoryChecks();
void UpdateAreas(RandomizerCheckArea area);
void UpdateAllOrdering();
void UpdateAllAreas();
void RecalculateAllAreaTotals();
void SpoilAreaFromCheck(RandomizerCheck rc);
void SpoilAreaFromCantObtain(RandomizerGet rg);
void RecalculateAvailableChecks(RandomizerRegion startingRegion = RR_ROOT, RandoAgeTime startingAgeTime = RAT_NONE);
void LoadFromPreset(const nlohmann::json& info);
bool AreAllSilversSpoiled();

// Bottom-screen (Thor) check tracker tab. Data access for the embedded view.
// Refreshes display settings from CVars and runs a pending availability
// recalculation. Returns true once a save is loaded and the tracker is ready.
bool EmbeddedBegin();
// True when check availability (logic) is being computed.
bool IsLogicShown();
const std::vector<RandomizerCheck>& GetChecksForArea(RandomizerCheckArea area);
RandomizerCheckArea GetCurrentTrackerArea();
RandomizerCheck GetLastCheckedLocation();
std::string GetCheckLabel(RandomizerCheck rc);
// Item name (or price) the player already knows for this check, empty if none.
std::string GetCheckItemText(RandomizerCheck rc);
void SetCheckSkippedByPlayer(RandomizerCheck rc, bool skipped);
// Which ages reach an available check in the last logic pass (the logic counts
// trips through the Temple of Time): bit 0 = child, bit 1 = adult. 0 when the
// check is not available or the age could not be told apart.
enum : uint8_t { CHECK_AGE_CHILD = 1, CHECK_AGE_ADULT = 2 };
uint8_t GetCheckReachAges(RandomizerCheck rc);
} // namespace CheckTracker
