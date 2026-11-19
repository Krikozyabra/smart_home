/**
 * @file ColorView.h
 * @brief Представление свойства цвета устройства.
 */

#pragma once

#include "devices/Device.h"
#include "devices/PropertyTags.h"
#include "devices/interfaces/IColor.h"

#include <stdexcept>

namespace smart_home {

/**
 * @brief Адаптирует свойство RGB к интерфейсу IColor.
 * @details Использует правила синхронизации Device.
 */
class ColorView final : public IColor {
  public:
    /**
     * @brief Привязывает представление к устройству.
     * @param device Устройство, которое должно существовать при каждом
     * обращении через представление.
     */
    explicit ColorView(Device &device) noexcept : device_(device) {}

    /**
     * @brief Возвращает цвет в формате RGB.
     * @throws std::logic_error Если свойство отсутствует или имеет другой тип.
     */
    Color getColor() const override {
        const auto color = device_.get(props::light::Rgb);
        if (!color)
            throw std::logic_error("Missing or invalid 'color.rgb' property");
        return *color;
    }

    /// @brief Устанавливает значение RGB в словаре устройства.
    void setColor(Color color) override {
        device_.set(props::light::Rgb, color);
    }

  private:
    Device &device_;
};

} // namespace smart_home
