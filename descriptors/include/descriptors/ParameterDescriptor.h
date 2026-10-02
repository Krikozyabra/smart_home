/**
 * @file ParameterDescriptor.h
 * @brief Описание структур метаданных и дескриптора параметров.
 */

#pragma once

#include "DescriptorTypes.h"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace smart_home {

/**
 * @struct BooleanMetadata
 * @brief Метаданные для логического параметра.
 */
struct BooleanMetadata {};

/**
 * @struct IntegerMetadata
 * @brief Метаданные для целочисленного параметра.
 */
struct IntegerMetadata {
    /**
     * @brief Минимальное допустимое значение.
     */
    int min;
    /**
     * @brief Максимальное допустимое значение.
     */
    int max;
    /**
     * @brief Единица измерения величины.
     */
    std::string unit;
};

/**
 * @struct FloatMetadata
 * @brief Метаданные для вещественного параметра с плавающей запятой.
 */
struct FloatMetadata {
    /**
     * @brief Минимальное допустимое значение.
     */
    double min;
    /**
     * @brief Максимальное допустимое значение.
     */
    double max;
    /**
     * @brief Единица измерения величины.
     */
    std::string unit;
};

/**
 * @struct ColorMetadata
 * @brief Метаданные для параметра цвета.
 */
struct ColorMetadata {
    /**
     * @brief Используемая цветовая модель (например, RGB, HSV).
     */
    std::string color_model;
};

/**
 * @struct StringMetadata
 * @brief Метаданные для строкового параметра.
 */
struct StringMetadata {
    /**
     * @brief Максимально допустимая длина строки.
     */
    std::size_t max_length;
};

/**
 * @brief Вариант, объединяющий структуры метаданных для всех поддерживаемых типов параметров.
 */
using ParameterMetadata =
    std::variant<BooleanMetadata, IntegerMetadata, FloatMetadata, ColorMetadata,
                 StringMetadata>;

/**
 * @struct ParameterDescriptor
 * @brief Описатель параметра операции или свойства.
 */
struct ParameterDescriptor {
    /**
     * @brief Уникальный идентификатор параметра.
     */
    std::string id;
    /**
     * @brief Отображаемое имя параметра.
     */
    std::string name;

    /**
     * @brief Возвращает тип значения параметра.
     * @return Тип значения параметра ValueType.
     */
    ValueType getValueType() const {
        return this->type;
    }

    /**
     * @brief Возвращает метаданные параметра.
     * @return Константная ссылка на метаданные ParameterMetadata.
     */
    const ParameterMetadata& getMetadata() const {
        return this->metadata;
    }

    /**
     * @brief Конструктор описателя параметра.
     * @param c_id Идентификатор параметра.
     * @param c_name Отображаемое имя параметра.
     * @param c_value_type Тип значения параметра.
     * @param c_metadata Метаданные параметра, соответствующие его типу.
     * @throws std::invalid_argument Если тип значения и переданная структура метаданных не соответствуют друг другу.
     */
    ParameterDescriptor(std::string c_id, std::string c_name,
                        ValueType c_value_type, ParameterMetadata c_metadata)
        : id(std::move(c_id)), name(std::move(c_name)), type(c_value_type),
          metadata(std::move(c_metadata)) {
        if(!validateMetadata()) throw std::invalid_argument("Type and metadata struct must equal");
    }

  private:
    /**
     * @brief Тип значения параметра.
     */
    ValueType type;
    /**
     * @brief Метаданные параметра.
     */
    ParameterMetadata metadata;

    /**
     * @brief Проверяет соответствие между типом значения и типом структуры в метаданных.
     * @return true, если структура метаданных соответствует установленному типу значения, иначе false.
     */
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
