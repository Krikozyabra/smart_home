#pragma once

#include <string>

namespace smart_home {

class Device {
  public:
    Device(unsigned int, std::string);
    virtual ~Device() = default;

    unsigned int getId() const;

    std::string getName() const; 
    void setName(std::string);

  private:
    std::string name;
    unsigned int id;
};

} // namespace smart_home
