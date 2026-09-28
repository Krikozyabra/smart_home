#pragma once

#include "DescriptorTypes.h"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace smart_home {

struct BooleanMetadata {};

struct IntegerMetadata {
    int min;
    int max;
    std::string unit;
};

struct FloatMetadata {
    double min;
    double max;
    std::string unit;
};

struct ColorMetadata {
    std::string color_model;
};

struct StringMetadata {
    std::size_t max_length;
};

using ParameterMetadata =
    std::variant<BooleanMetadata, IntegerMetadata, FloatMetadata, ColorMetadata,
                 StringMetadata>;

struct ParameterDescriptor {
    std::string id;
    std::string name;

    ValueType getValueType() const {
        return this->type;
    }

    const ParameterMetadata& getMetadata() const {
        return this->metadata;
    }

    ParameterDescriptor(std::string c_id, std::string c_name,
                        ValueType c_value_type, ParameterMetadata c_metadata)
        : id(std::move(c_id)), name(std::move(c_name)), type(c_value_type),
          metadata(std::move(c_metadata)) {
        if(!validateMetadata()) throw std::invalid_argument("Type and metadata struct must equal");
    }

  private:
    ValueType type;
    ParameterMetadata metadata;
    
    bool validateMetadata() const {
        switch (this->type) {
        case ValueType::Boolean:
            return std::holds_alternative<BooleanMetadata>(metadata);
        case ValueType::Color:
            return std::holds_alternative<ColorMetadata>(metadata);
        case ValueType::Float:
            return std::holds_alternative<FloatMetadata>(metadata);
        case ValueType::Integer:
            return std::holds_alternative<IntegerMetadata>(metadata);
        case ValueType::String:
            return std::holds_alternative<StringMetadata>(metadata);
        }
        return false;
    }
};

} // namespace smart_home
