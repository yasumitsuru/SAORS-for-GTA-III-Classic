/*
    This project-owned compile probe includes, but does not copy or redistribute,
    the pinned plugin-sdk headers. The upstream zlib-style notice remains in the
    external checkout and is recorded in THIRD_PARTY_NOTICES.md.
*/

#include <CMenuManager.h>
#include <Events.h>
#include <cDMAudio.h>
#include <common.h>

#include <cstddef>

static_assert(sizeof(void*) == 4U, "plugin_III must be compiled for Windows x86");
static_assert(offsetof(CMenuManager, m_bMenuActive) == 0x111U);
static_assert(offsetof(CMenuManager, m_bGameNotLoaded) == 0x116U);

int saorsPluginSdkCompileProbe() {
    const auto findPlayerVehicle = &FindPlayerVehicle;
    const auto getRadioInCar = &cDMAudio::GetRadioInCar;
    const auto* processEvent = &plugin::Events::gameProcessEvent;
    return findPlayerVehicle != nullptr && getRadioInCar != nullptr && processEvent != nullptr ? 0
                                                                                               : 1;
}
