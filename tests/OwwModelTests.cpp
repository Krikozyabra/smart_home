#include "TestUtil.h"
#include "oww/OpenWakeWordDetector.h"

#include <chrono>
#include <cmath>
#include <iostream>

// Optional smoke/resource check for an actual trained binary OpenWakeWord
// model. This checks execution and sustained streaming, not detection accuracy.
int main(int argc, char **argv) {
    using namespace smart_home::oww;
    try {
        require(argc == 4,
                "expected classifier, melspectrogram and embedding paths");
        OwwConfig config;
        config.model_path = argv[1];
        config.melspec_model_path = argv[2];
        config.embedding_model_path = argv[3];
        OpenWakeWordDetector detector(config);
        PcmFrame frame{};
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; ++i) {
            if (i % 100 == 0)
                for (std::size_t sample = 0; sample < frame.size(); ++sample)
                    frame[sample] = static_cast<std::int16_t>(
                        static_cast<int>((sample * 127 + i * 17) % 2001) -
                        1000);
            const float score = detector.processFrame(frame);
            require(std::isfinite(score) && score >= 0 && score <= 1,
                    "invalid probability");
        }
        const auto elapsed = std::chrono::duration<double>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
        std::cout << "1000 frames (80 seconds of audio), " << elapsed
                  << " seconds CPU wall time\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
