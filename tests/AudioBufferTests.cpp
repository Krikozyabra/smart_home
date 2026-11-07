#include "AudioTestSupport.h"
#include <limits>

using namespace audio_test;
void tests() {
    auto control = std::make_shared<Control>();
    auto capture = create(tiny(), control);
    require(capture->snapshot().frames.empty(), "empty before start");
    capture->startContinuous();
    control->awaitIdleRead(0);
    const auto session = capture->snapshot().session_id;
    capture->startContinuous();
    require(capture->snapshot().session_id == session && session == 1,
            "idempotent start");
    control->push(2, ramp(2));
    control->awaitIdleRead(2, 2);
    require(capture->snapshot().frames.empty(), "partial frame invisible");
    control->push(38, ramp(38, 2));
    control->awaitIdleRead(40, 3);
    auto snapshot = capture->snapshot();
    require(snapshot.frames.size() == 3, "bounded ring");
    for (unsigned i = 0; i < 3; ++i) {
        require(snapshot.frames[i].sequence == 7 + i,
                "sequence after wrapping");
        require(snapshot.frames[i].samples == ramp(4, (7 + i) * 4),
                "complete PCM after wrapping");
    }
    require(snapshot.frames.front().sequence > 0, "consumer sees gap");
    snapshot.frames[0].samples[0] = 42;
    require(capture->snapshot().frames[0].samples[0] != 42,
            "independent snapshot");
    control->push(1, {99});
    control->awaitIdleRead(41, 4);
    capture->stopContinuous();
    require(capture->snapshot().frames.back().sequence == 9,
            "discard partial at stop");
    auto last = capture->snapshot();
    control->push(2, {7, 8});
    require(capture->recordFor(2ms) == PcmSamples({7, 8}),
            "one-shot after continuous");
    require(capture->snapshot().frames.back().samples ==
                last.frames.back().samples,
            "one-shot preserves old ring");
    capture->startContinuous();
    control->awaitIdleRead(43, 5);
    require(capture->snapshot().session_id == session + 1 &&
                capture->snapshot().frames.empty(),
            "restart clears ring and changes session");
    control->push(4, {1, 2, 3, 4});
    control->awaitIdleRead(47, 6);
    capture->stopContinuous();
    require(capture->snapshot().frames[0].sequence == 0,
            "restart sequence starts at zero");
    detail::TestAccess::setCounters(
        *capture, std::numeric_limits<std::uint64_t>::max(), 0);
    expectException<std::runtime_error>([&] { capture->startContinuous(); });
    capture->rethrowIfFailed();
    require(capture->snapshot().frames.size() == 1,
            "session overflow leaves old snapshot");

    auto overflow_control = std::make_shared<Control>();
    auto overflow = create(tiny(), overflow_control);
    overflow->startContinuous();
    overflow_control->awaitIdleRead(0);
    detail::TestAccess::setCounters(*overflow, 1,
                                    std::numeric_limits<std::uint64_t>::max());
    overflow_control->push(8, ramp(8));
    overflow_control->await([&] { return overflow_control->finishes > 0; });
    expectException<std::runtime_error>([&] { overflow->stopContinuous(); });
    require(overflow->snapshot().frames.size() == 1 &&
                overflow->snapshot().frames[0].sequence ==
                    std::numeric_limits<std::uint64_t>::max(),
            "sequence overflow never wraps");
}
int main() { return run(tests); }
