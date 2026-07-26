#include "saors_gta3/StreamManager.hpp"

#include <algorithm>
#include <utility>

namespace saors {

StreamManager::StreamManager(std::unique_ptr<AudioBackend> backend)
    : backend_(backend ? std::move(backend) : createNullAudioBackend()) {}

bool StreamManager::start(const std::string& url, const float volume) {
    const std::lock_guard<std::mutex> lock(mutex_);

    backend_->stop();
    currentUrl_.clear();
    lastError_.clear();
    volume_ = std::clamp(volume, 0.0F, 1.0F);

    if (!backend_->setBufferMilliseconds(bufferMilliseconds_) || !backend_->setVolume(volume_) ||
        !backend_->open(url) || !backend_->play()) {
        lastError_ = backend_->lastError();
        backend_->stop();
        return false;
    }

    currentUrl_ = url;
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
        lastError_.clear();
    } catch (...) {
        // The host game must remain stable even during teardown.
    }
}

bool StreamManager::reconnect() {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (currentUrl_.empty()) {
        lastError_ = "no stream is available to reconnect";
        return false;
    }

    const auto url = currentUrl_;
    backend_->stop();
    if (!backend_->setBufferMilliseconds(bufferMilliseconds_) || !backend_->setVolume(volume_) ||
        !backend_->open(url) || !backend_->play()) {
        lastError_ = backend_->lastError();
        backend_->stop();
        return false;
    }
    lastError_.clear();
    return true;
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
