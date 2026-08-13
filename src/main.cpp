#include "Device.h"
#include "Light.h"
#include "TemperatureSensor.h"
#include "interfaces/IBrightness.h"
#include "interfaces/IColor.h"
#include "interfaces/IOnOff.h"
#include "utility/color_type.h"
#include <iostream>

int main(int argc, char *argv[]) {
    Color c1{100, 120, 100};
    Device *light = new Light(0, "Light1", true, 0, c1);

    IOnOff *power = dynamic_cast<IOnOff *>(light);
    if (power != nullptr) {
        power->setOn(true);
        std::cout << "Light's on state = " << power->isOn() << std::endl;
    }

    IBrightness *brightness = dynamic_cast<IBrightness *>(light);
    if (brightness != nullptr) {
        brightness->setBrightness(50);
        std::cout << "Light's brightness set on " << brightness->getBrightness()
                  << std::endl;
    }

    IColor *color = dynamic_cast<IColor *>(light);
    if (color != nullptr) {
        Color c2{250, 0, 0};
        color->setColor(c2);
        std::cout << "Light's color is " << color->getColor().red << " red, "
                  << color->getColor().green << " green, "
                  << color->getColor().blue << " blue\n";
    }

    Device *ts = new TemperatureSensor(1, "TS1");
    IOnOff *ts_power = dynamic_cast<IOnOff*>(ts);
    if (ts_power == nullptr) std::cout << "TS doesnt have the IOnOff\n";
    IBrightness *ts_brightness = dynamic_cast<IBrightness*>(ts);
    if (ts_brightness == nullptr) std::cout << "TS doesnt have the IBrightness\n";
    IColor *ts_color = dynamic_cast<IColor*>(ts);
    if (ts_color == nullptr) std::cout << "TS doesnt have the IColor\n";
    return 0;
}
