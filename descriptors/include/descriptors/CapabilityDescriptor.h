/**
 * @file CapabilityDescriptor.h
 * @brief Определение класса описателя возможности CapabilityDescriptor.
 */

#pragma once

#include "OperationDescriptor.h"
#include <string>
#include <utility>
#include <vector>

namespace smart_home {

/**
 * @brief Описатель возможности устройства умного дома.
 */
class CapabilityDescriptor {
    /**
     * @brief Уникальный идентификатор возможности.
     */
    std::string id;
    /**
     * @brief Человекочитаемое наименование возможности.
     */
    std::string name;

    /**
     * @brief Список описателей операций, поддерживаемых возможностью.
     */
    std::vector<OperationDescriptor> operations;

  public:
    /**
     * @brief Конструктор описателя возможности.
     * @param c_id Идентификатор возможности.
     * @param c_name Наименование возможности.
     * @param c_operations Список описателей операций возможности.
     */
    CapabilityDescriptor(std::string c_id, std::string c_name,
                         std::vector<OperationDescriptor> c_operations)
        : id(std::move(c_id)), name(std::move(c_name)),
          operations(std::move(c_operations)) {}

    /**
     * @brief Получает идентификатор возможности.
     * @return Константная ссылка на строку с идентификатором возможности.
     */
    const std::string& getId() const {
        return this->id;
    }

    /**
     * @brief Получает наименование возможности.
     * @return Константная ссылка на строку с наименованием возможности.
     */
    const std::string& getName() const {
        return this->name;
    }

    /**
     * @brief Получает список описателей операций возможности.
     * @return Константная ссылка на вектор описателей операций.
     */
    const std::vector<OperationDescriptor>& getOperations() const {
        return this->operations;
    }
};

} // namespace smart_home
