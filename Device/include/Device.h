#pragma once

#include <string>
#include <utility>

class Device {
  public:
    Device(unsigned int c_id, std::string c_name) : id(c_id), name(std::move(c_name)) {}

    unsigned int getId() const { return this->id; }

    std::string getName() const { return this->name; }
    void setName(std::string new_name) { this->name = std::move(new_name); }

  private:
    std::string name;
    unsigned int id;
};
