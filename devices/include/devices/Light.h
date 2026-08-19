#pragma once

#include "Device.h"
#include "devices/interfaces/IBrightness.h"
#include "devices/interfaces/IColor.h"
#include "devices/interfaces/IOnOff.h"
#include "types/Color.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace smart_home {

class Light : public Device, public IOnOff, public IBrightness, public IColor {
  public:
    Light(unsigned int c_id, std::string c_name, bool c_isOn,
          std::uint8_t c_brightness, struct Color c_color)
        : Device(c_id, std::move(c_name)), isOn_state(c_isOn),
          brightness(c_brightness), color(c_color) {}

    bool isOn() const override { return this->isOn_state; }
    void setOn(bool new_isOn) override { this->isOn_state = new_isOn; }

    std::uint8_t getBrightness() const override { return this->brightness; }
    void setBrightness(std::uint8_t new_brightness) override {
        if (new_brightness > 100)
            throw std::invalid_argument(
                "Light's brightness should be in range 0 and 100");

        this->brightness = new_brightness;
    }

    struct Color getColor() const override { return this->color; }
    void setColor(struct Color new_color) override { this->color = new_color; }

  private:
    bool isOn_state;
    std::uint8_t brightness;
    struct Color color;
};

} // namespace smart_home
