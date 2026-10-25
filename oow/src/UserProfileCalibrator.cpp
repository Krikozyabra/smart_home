#include "oow/UserProfileCalibrator.h"
#include "logging/Logging.h"
#include "oow/OOWErrors.h"
#include <cmath>

namespace smart_home::oow {

void UserProfileCalibrator::calibrate(const std::vector<AudioClip> &samples) {
    if (samples.size() != kRequiredCalibrationClips)
        throw std::invalid_argument(
            "Expected exactly 5 audio clips for calibration");

    // Проверяем весь ввод до обращения к детектору: частичная проверка не
    // профиль.
    for (const auto &clip : samples) {
        if (clip.samples.empty() ||
            clip.format.sample_rate != kDefaultSampleRateHz ||
            clip.format.channels != 1 || clip.format.bits_per_sample != 16)
            throw std::invalid_argument(
                "Expected nonempty 16 kHz 16-bit mono audio clips");
    }
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto confidence = detector_.processClip(samples[i]);
        if (!std::isfinite(confidence) || confidence < 0 || confidence > 1)
            throw OOWException(
                "Некорректная уверенность детектора при калибровке");
        if (confidence < kCalibrationThreshold) {
            MissingKeywordException error(i, confidence);
            logging::log(logging::Level::Error, "OOW", error.what());
            throw error;
        }
    }
    logging::log(
        logging::Level::Info, "OOW",
        "Калибровка профиля пользователя успешно завершена по 5 записям");
}

} // namespace smart_home::oow
