/**
 * @file DescriptorTypes.h
 * @brief Перечисления типов операций и типов значений для описателей устройств.
 */

#pragma once

#include <cstdint>
namespace smart_home{

/**
 * @brief Тип операции, поддерживаемой устройством.
 */
enum class OperationType : std::uint8_t {
    /**
     * @brief Операция чтения состояния или свойства.
     */
    Read,
    /**
     * @brief Операция записи данных или изменения состояния.
     */
    Write,
    /**
     * @brief Событие, генерируемое устройством.
     */
    Event
};

/**
 * @brief Тип данных значения свойства или аргумента.
 */
enum class ValueType : std::uint8_t {
    /**
     * @brief Логический тип данных (булево значение).
     */
    Boolean,
    /**
     * @brief Целочисленный тип данных.
     */
    Integer,
    /**
     * @brief Числовой тип данных с плавающей запятой.
     */
    Float,
    /**
     * @brief Тип данных, представляющий цвет.
     */
    Color,
    /**
     * @brief Строковый тип данных.
     */
    String
};

}
