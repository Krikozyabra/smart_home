#pragma once

#include "oow/interfaces/IOOWDetector.h"
#include <atomic>
#include <functional>
#include <stdexcept>

namespace smart_home::oow {

class MockOOWDetector final : public IOOWDetector {
  public:
    std::vector<float> feed_confidences;
    std::vector<float> clip_confidences;
    std::atomic<size_t> feeds{0};
    std::atomic<size_t> clips{0};
    std::atomic<size_t> resets{0};
    bool fail_feed = false;
    std::function<void()> on_clip;

    DetectionResult feed(const int16_t *, size_t) override {
        const auto index = feeds++;
        if (fail_feed)
            throw std::runtime_error("mock detector failure");
        // detected не участвует в сравнении с порогом: сервис использует
        // confidence.
        return {false,
                index < feed_confidences.size() ? feed_confidences[index]
                                                : 0.0f,
                "Джарвис"};
    }
    float processClip(const AudioClip &) override {
        const auto index = clips++;
        if (on_clip)
            on_clip();
        return index < clip_confidences.size() ? clip_confidences[index] : 0.9f;
    }
    void reset() override { ++resets; }
};

} // namespace smart_home::oow
