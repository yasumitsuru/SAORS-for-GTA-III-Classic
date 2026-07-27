#include "saors_gta3/RadioController.hpp"

#include "saors_gta3/GameIntegration.hpp"
#include "saors_gta3/Logger.hpp"
#include "saors_gta3/StreamManager.hpp"

#include <algorithm>

namespace saors {

RadioController::RadioController(GameIntegration& game, StreamManager& streams,
                                 const ConfigurationData& configuration)
    : game_(game), streams_(streams), configuration_(configuration), stationKeys_({
                                                                         {0, "HeadRadio"},
                                                                         {1, "DoubleClef"},
                                                                         {2, "KJah"},
                                                                         {3, "RiseFM"},
                                                                         {4, "Lips106"},
                                                                         {5, "GameFM"},
                                                                         {6, "MSXFM"},
                                                                         {7, "Flashback"},
                                                                         {8, "Chatterbox"},
                                                                     }) {}

void RadioController::update() {
    // TODO(clean-room-hooks): this method becomes active only after verified GTA III
    // addresses and calling conventions are documented and reviewed.
    if (!game_.isPlayerInVehicle() || game_.isPauseMenuActive()) {
        restoreOriginalRadio();
        return;
    }

    const int stationId = game_.currentStation();
    const auto* station = stationForGameId(stationId);
    if (station == nullptr || !station->enabled || station->url.empty()) {
        restoreOriginalRadio();
        return;
    }

    if (stationId != activeStationId_) {
        const float volume =
            std::clamp(game_.radioVolume() * configuration_.general.volumeMultiplier, 0.0F, 1.0F);
        ResolveOptions options;
        options.allowHttpStreams = station->allowHttp;
        options.connectTimeoutMilliseconds =
            configuration_.general.playlistConnectTimeoutMilliseconds;
        options.receiveTimeoutMilliseconds =
            configuration_.general.playlistReceiveTimeoutMilliseconds;
        options.maximumPlaylistBytes = configuration_.general.playlistMaximumBytes;
        options.maximumEntries = configuration_.general.playlistMaximumEntries;
        options.maximumRedirects = configuration_.general.playlistMaximumRedirects;
        options.maximumDepth = configuration_.general.playlistMaximumDepth;
        const bool started = configuration_.general.resolveRemotePlaylists
                                 ? streams_.startConfiguredUrl(station->url, volume, options)
                                 : streams_.start(station->url, volume);
        if (started) {
            activeStationId_ = stationId;
        } else {
            Logger::warning("Online station could not be started; original radio remains active");
            restoreOriginalRadio();
        }
    }
}

void RadioController::restoreOriginalRadio() noexcept {
    if (activeStationId_ >= 0) {
        streams_.stop();
        activeStationId_ = -1;
    }
    // The original game radio is never muted in this milestone, so no game memory
    // needs to be restored here.
}

const StationConfiguration* RadioController::stationForGameId(const int stationId) const {
    const auto key = stationKeys_.find(stationId);
    if (key == stationKeys_.end()) {
        return nullptr;
    }
    const auto station = configuration_.stations.find(key->second);
    return station == configuration_.stations.end() ? nullptr : &station->second;
}

} // namespace saors
