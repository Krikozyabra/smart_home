/**
 * @file TemperatureView.h
 * @brief Представление свойства температуры устройства.
 */

#pragma once

#include "devices/Device.h"
#include "devices/PropertyTags.h"
#include "devices/interfaces/ITemperature.h"

#include <stdexcept>

namespace smart_home {

/**
 * @brief Адаптирует свойство температуры к интерфейсу ITemperature.
 * @details Использует правила синхронизации Device.
 */
class TemperatureView final : public ITemperature {
  public:
    /**
     * @brief Привязывает представление к устройству для чтения.
     * @param device Устройство, которое должно существовать при каждом
     * обращении через представление.
     */
    explicit TemperatureView(const Device &device) noexcept : device_(device) {}

    TemperatureView(const Device &&) = delete;

    /**
     * @brief Возвращает температуру в градусах Цельсия.
     * @throws std::logic_error Если свойство отсутствует или имеет другой тип.
     */
    double getTemperature() const override {
        const auto temperature = device_.get(props::climate::Temperature);
        if (!temperature)
            throw std::logic_error(
                "Missing or invalid 'temperature.celsius' property");
        return *temperature;
    }

  private:
    const Device &device_;
};

} // namespace smart_home
