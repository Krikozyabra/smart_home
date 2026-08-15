#include "Device.h"
#include "drivers/SimulatedLightDriver.h"
#include "drivers/interfaces/IDeviceDriver.h"
#include "interfaces/IBrightness.h"

#include <memory>
#include <iostream>

int main(int argc, char *argv[]) {
    std::unique_ptr<smart_home::IDeviceDriver> simulated_light =
        std::make_unique<smart_home::SimulatedLightDriver>(0, "AC:12:BD",
                                                           "Light1");

    Device &simulated_device = simulated_light->device();

    auto* brightness = dynamic_cast<IBrightness*>(&simulated_device);

    if(brightness != nullptr){
        brightness->setBrightness(100);
        std::cout << "The simulated light's brightness is " << static_cast<int>(brightness->getBrightness()) << std::endl;
    }else {
        std::cout << "The device doesn't have the brightness\n";
    }
    return 0;
}
