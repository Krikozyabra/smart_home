/**
 * @file OnOffView.h
 * @brief Представление свойства включения устройства.
 */

#pragma once

#include "devices/Device.h"
#include "devices/PropertyTags.h"
#include "devices/interfaces/IOnOff.h"

#include <stdexcept>

namespace smart_home {

/**
 * @brief Адаптирует свойство включения к интерфейсу IOnOff.
 * @details Использует правила синхронизации Device.
 */
class OnOffView final : public IOnOff {
  public:
    /**
     * @brief Привязывает представление к устройству.
     * @param device Устройство, которое должно существовать при каждом
     * обращении через представление.
     */
    explicit OnOffView(Device &device) noexcept : device_(device) {}

    /**
     * @brief Возвращает состояние включения.
     * @throws std::logic_error Если свойство отсутствует или имеет другой тип.
     */
    bool isOn() const override {
        const auto state = device_.get(props::light::State);
        if (!state)
            throw std::logic_error(
                "Missing or invalid 'on_off.state' property");
        return *state;
    }

    /// @brief Устанавливает состояние включения в словаре устройства.
    void setOn(bool on) override { device_.set(props::light::State, on); }

  private:
    Device &device_;
};

} // namespace smart_home
