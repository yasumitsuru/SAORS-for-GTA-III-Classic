#include "saors_gta3/StreamManager.hpp"

#include <algorithm>
#include <utility>

namespace saors {

StreamManager::StreamManager(std::unique_ptr<AudioBackend> backend,
                             std::shared_ptr<PlaylistResolver> resolver)
    : backend_(backend ? std::move(backend) : createNullAudioBackend()),
      resolver_(std::move(resolver)) {}

bool StreamManager::start(const std::string& url, const float volume) {
    const std::lock_guard<std::mutex> lock(mutex_);

    backend_->stop();
    currentUrl_.clear();
    configuredUrl_.clear();
    lastResolution_.reset();
    lastError_.clear();
    volume_ = std::clamp(volume, 0.0F, 1.0F);
    return startMediaLocked(url);
}

bool StreamManager::startConfiguredUrl(const std::string& configuredUrl, const float volume,
                                       const ResolveOptions& options) {
    const std::lock_guard<std::mutex> lock(mutex_);

    backend_->stop();
    currentUrl_.clear();
    configuredUrl_ = configuredUrl;
    lastResolution_.reset();
    lastError_.clear();
    volume_ = std::clamp(volume, 0.0F, 1.0F);
    resolveOptions_ = options;

    if (!resolver_) {
        lastError_ = "remote playlist resolver is unavailable";
        return false;
    }
    const auto resolved = resolver_->resolve(configuredUrl, options);
    if (!resolved) {
        lastError_ = resolved.error;
        return false;
    }
    lastResolution_ = resolved.value;
    if (!startMediaLocked(resolved.value.mediaUrl)) {
        lastResolution_.reset();
        return false;
    }
    return true;
}

bool StreamManager::startMediaLocked(const std::string& url) {
    if (!backend_->setBufferMilliseconds(bufferMilliseconds_) || !backend_->open(url) ||
        !backend_->setVolume(volume_) || !backend_->play()) {
        lastError_ = backend_->lastError();
        backend_->stop();
        return false;
    }

    currentUrl_ = url;
    lastError_.clear();
    return true;
}

bool StreamManager::pause() {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!backend_->pause()) {
        lastError_ = backend_->lastError();
        return false;
    }
    return true;
}

bool StreamManager::resume() {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!backend_->play()) {
        lastError_ = backend_->lastError();
        return false;
    }
    return true;
}

void StreamManager::stop() noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        backend_->stop();
        currentUrl_.clear();
        configuredUrl_.clear();
        lastResolution_.reset();
        lastError_.clear();
    } catch (...) {
        // The host game must remain stable even during teardown.
    }
}

bool StreamManager::setVolume(const float volume) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto clamped = std::clamp(volume, 0.0F, 1.0F);
    if (!backend_->setVolume(clamped)) {
        lastError_ = backend_->lastError();
        return false;
    }
    volume_ = clamped;
    lastError_.clear();
    return true;
}

bool StreamManager::reconnect() {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (currentUrl_.empty() && configuredUrl_.empty()) {
        lastError_ = "no stream is available to reconnect";
        return false;
    }

    const auto directUrl = currentUrl_;
    backend_->stop();
    currentUrl_.clear();

    std::string mediaUrl;
    if (!configuredUrl_.empty()) {
        if (!resolver_) {
            lastError_ = "remote playlist resolver is unavailable";
            return false;
        }
        const auto resolved = resolver_->resolve(configuredUrl_, resolveOptions_);
        if (!resolved) {
            lastError_ = resolved.error;
            lastResolution_.reset();
            return false;
        }
        mediaUrl = resolved.value.mediaUrl;
        lastResolution_ = resolved.value;
    } else {
        mediaUrl = directUrl;
    }
    return startMediaLocked(mediaUrl);
}

bool StreamManager::setBufferMilliseconds(const std::uint32_t milliseconds) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (milliseconds == 0 || !backend_->setBufferMilliseconds(milliseconds)) {
        lastError_ =
            milliseconds == 0 ? "buffer duration must be greater than zero" : backend_->lastError();
        return false;
    }
    bufferMilliseconds_ = milliseconds;
    lastError_.clear();
    return true;
}

AudioState StreamManager::state() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        return backend_->state();
    } catch (...) {
        return AudioState::error;
    }
}

std::string StreamManager::lastError() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

std::string StreamManager::currentUrl() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return currentUrl_;
}

std::string StreamManager::configuredUrl() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return configuredUrl_;
}

std::optional<ResolvedStream> StreamManager::lastResolution() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return lastResolution_;
}

std::uint32_t StreamManager::bufferMilliseconds() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        return bufferMilliseconds_;
    } catch (...) {
        return 0;
    }
}

const char* StreamManager::backendName() const noexcept {
    return backend_->name();
}

} // namespace saors
