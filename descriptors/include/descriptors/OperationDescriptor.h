/**
 * @file OperationDescriptor.h
 * @brief Описание дескриптора операции устройства.
 */

#pragma once

#include "DescriptorTypes.h"
#include "ParameterDescriptor.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace smart_home {

/**
 * @class OperationDescriptor
 * @brief Описатель операции устройства или его функциональной возможности.
 */
class OperationDescriptor {
    /**
     * @brief Уникальный идентификатор операции.
     */
    std::string id;
    /**
     * @brief Отображаемое имя операции.
     */
    std::string name;
    /**
     * @brief Тип операции.
     */
    OperationType type;

    /**
     * @brief Список входных параметров операции.
     */
    std::vector<ParameterDescriptor> input;
    /**
     * @brief Выходной параметр операции, если предусмотрен.
     */
    std::optional<ParameterDescriptor> output;

  public:
    /**
     * @brief Конструктор описателя операции.
     * @param c_id Идентификатор операции.
     * @param c_name Отображаемое имя операции.
     * @param c_type Тип операции.
     * @param c_input Вектор входных параметров.
     * @param c_output Необязательный выходной параметр операции.
     */
    OperationDescriptor(std::string c_id, std::string c_name,
                        OperationType c_type,
                        std::vector<ParameterDescriptor> c_input,
                        std::optional<ParameterDescriptor> c_output)
        : id(std::move(c_id)), name(std::move(c_name)), type(c_type),
          input(std::move(c_input)), output(std::move(c_output)) {}

    /**
     * @brief Возвращает идентификатор операции.
     * @return Константная ссылка на строку с идентификатором операции.
     */
    const std::string& getId() const {
        return this->id;
    }

    /**
     * @brief Возвращает имя операции.
     * @return Константная ссылка на строку с именем операции.
     */
    const std::string& getName() const {
        return this->name;
    }

    /**
     * @brief Возвращает тип операции.
     * @return Тип операции OperationType.
     */
    OperationType getType() const {
        return this->type;
    }

    /**
     * @brief Возвращает список входных параметров операции.
     * @return Константная ссылка на вектор входных параметров.
     */
    const std::vector<ParameterDescriptor>& getInputParameters() const {
        return this->input;
    }

    /**
     * @brief Возвращает выходной параметр операции.
     * @return Константная ссылка на optional, содержащий выходной параметр при наличии.
     */
    const std::optional<ParameterDescriptor>& getOutputParameter() const {
        return this->output;
    }
};

} // namespace smart_home
