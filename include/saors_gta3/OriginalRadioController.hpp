#pragma once

#include "saors_gta3/ExecutableProfile.hpp"

#include <memory>

namespace saors {

class OriginalRadioController {
  public:
    virtual ~OriginalRadioController() = default;

    [[nodiscard]] virtual bool available() const noexcept = 0;
    virtual bool mute() noexcept = 0;
    virtual bool restore() noexcept = 0;
    [[nodiscard]] virtual bool muted() const noexcept = 0;
};

class NullOriginalRadioController final : public OriginalRadioController {
  public:
    [[nodiscard]] bool available() const noexcept override;
    bool mute() noexcept override;
    bool restore() noexcept override;
    [[nodiscard]] bool muted() const noexcept override;
};

struct OriginalRadioSuppressionPolicy {
    bool buildEnabled{false};
    bool gameplayExecutorEnabled{false};
    bool runtimeEnabled{false};
    ExecutableProfileId executableProfile{ExecutableProfileId::unsupported};
};

[[nodiscard]] bool
shouldEnableOriginalRadioSuppression(const OriginalRadioSuppressionPolicy& policy,
                                     const OriginalRadioController& controller) noexcept;

[[nodiscard]] std::unique_ptr<OriginalRadioController>
createOriginalRadioController(ExecutableProfileId executableProfile);

} // namespace saors
