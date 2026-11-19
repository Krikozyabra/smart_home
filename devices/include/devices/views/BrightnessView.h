/**
 * @file BrightnessView.h
 * @brief Представление свойства яркости устройства.
 */

#pragma once

#include "devices/Device.h"
#include "devices/PropertyTags.h"
#include "devices/interfaces/IBrightness.h"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace smart_home {

/**
 * @brief Адаптирует свойство яркости к интерфейсу IBrightness.
 * @details Использует правила синхронизации Device.
 */
class BrightnessView final : public IBrightness {
  public:
    /**
     * @brief Привязывает представление к устройству.
     * @param device Устройство, которое должно существовать при каждом
     * обращении через представление.
     */
    explicit BrightnessView(Device &device) noexcept : device_(device) {}

    /**
     * @brief Возвращает яркость как std::uint8_t.
     * @throws std::logic_error Если свойство отсутствует, имеет другой тип
     * или его значение нельзя представить как std::uint8_t.
     */
    std::uint8_t getBrightness() const override {
        const auto brightness = device_.get(props::light::Brightness);
        if (!brightness || *brightness < 0 ||
            *brightness > std::numeric_limits<std::uint8_t>::max())
            throw std::logic_error(
                "Missing or invalid 'brightness.level' property");
        return static_cast<std::uint8_t>(*brightness);
    }

    /**
     * @brief Устанавливает яркость в процентах.
     * @throws std::invalid_argument Если яркость выше 100.
     */
    void setBrightness(std::uint8_t brightness) override {
        device_.set(props::light::Brightness, static_cast<int>(brightness));
    }

  private:
    Device &device_;
};

} // namespace smart_home
