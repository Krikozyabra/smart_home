/**
 * @file Light.h
 * @brief Заголовочный файл класса умного источника света Light.
 */

#pragma once

#include "Device.h"
#include "common/DeviceId.h"
#include "devices/interfaces/IBrightness.h"
#include "devices/interfaces/IColor.h"
#include "devices/interfaces/IOnOff.h"
#include "types/Color.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace smart_home {

/**
 * @brief Класс умного источника света.
 *
 * Управляет состоянием включения/выключения, уровнем яркости и цветом освещения.
 */
class Light : public Device, public IOnOff, public IBrightness, public IColor {
  public:
    /**
     * @brief Конструктор умного источника света.
     * @param c_id Уникальный идентификатор устройства.
     * @param c_name Название устройства.
     * @param c_isOn Начальное состояние включения (true — включен, false — выключен).
     * @param c_brightness Начальный уровень яркости; конструктор не проверяет диапазон.
     * @param c_color Начальный цвет освещения.
     */
    Light(DeviceId c_id, std::string c_name, bool c_isOn,
          std::uint8_t c_brightness, struct Color c_color)
        : Device(c_id, std::move(c_name)), isOn_state(c_isOn),
          brightness(c_brightness), color(c_color) {}

    /**
     * @brief Проверяет, включен ли источник света.
     * @return true, если источник света включен, иначе false.
     */
    bool isOn() const override { return this->isOn_state; }

    /**
     * @brief Устанавливает состояние включения источника света.
     * @param new_isOn Новое состояние включения (true — включить, false — выключить).
     */
    void setOn(bool new_isOn) override { this->isOn_state = new_isOn; }

    /**
     * @brief Возвращает текущий уровень яркости источника света.
     * @return Сохраненный уровень яркости.
     */
    std::uint8_t getBrightness() const override { return this->brightness; }

    /**
     * @brief Устанавливает уровень яркости источника света.
     * @param new_brightness Новый уровень яркости (от 0 до 100).
     * @throws std::invalid_argument Если значение new_brightness превышает 100.
     */
    void setBrightness(std::uint8_t new_brightness) override {
        if (new_brightness > 100)
            throw std::invalid_argument(
                "Light's brightness should be in range 0 and 100");

        this->brightness = new_brightness;
    }

    /**
     * @brief Возвращает текущий цвет источника света.
     * @return Текущий цвет в виде структуры Color.
     */
    struct Color getColor() const override { return this->color; }

    /**
     * @brief Устанавливает цвет источника света.
     * @param new_color Новый цвет освещения.
     */
    void setColor(struct Color new_color) override { this->color = new_color; }

  private:
    /**
     * @brief Текущее состояние включения источника света.
     */
    bool isOn_state;

    /**
     * @brief Текущий уровень яркости источника света.
     */
    std::uint8_t brightness;

    /**
     * @brief Текущий цвет источника света.
     */
    struct Color color;
};

} // namespace smart_home
