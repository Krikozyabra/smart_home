#pragma once

#include "drivers/factories/IDeviceDriverFactory.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
namespace smart_home {

class DriverFactoryRegistry {
  private:
    std::unordered_map<std::string, std::unique_ptr<IDeviceDriverFactory>>
        factories;

  public:
    void add(std::unique_ptr<IDeviceDriverFactory>);

    IDeviceDriverFactory *find(const std::string &driver_id);
    const IDeviceDriverFactory *find(const std::string &driver_id) const;

    std::size_t size() const noexcept;
};

} // namespace smart_home
