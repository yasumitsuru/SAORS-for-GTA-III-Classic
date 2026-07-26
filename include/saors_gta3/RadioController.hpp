#pragma once

#include "saors_gta3/Configuration.hpp"

#include <map>
#include <string>

namespace saors {

class GameIntegration;
class StreamManager;

class RadioController {
  public:
    RadioController(GameIntegration& game, StreamManager& streams,
                    const ConfigurationData& configuration);

    void update();
    void restoreOriginalRadio() noexcept;

  private:
    [[nodiscard]] const StationConfiguration* stationForGameId(int stationId) const;

    GameIntegration& game_;
    StreamManager& streams_;
    const ConfigurationData& configuration_;
    std::map<int, std::string> stationKeys_;
    int activeStationId_{-1};
};

} // namespace saors
