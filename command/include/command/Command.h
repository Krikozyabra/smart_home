/**
 * @file Command.h
 * @brief Определение класса Command для представления команд управления устройствами.
 */

#pragma once

#include "command/Value.h"
#include "common/DeviceId.h"

#include <string>
#include <vector>

namespace smart_home {

/**
 * @brief Класс команды, предназначенный для выполнения операции над устройством.
 */
class Command {
private:
    /**
     * @brief Идентификатор целевого устройства.
     */
    DeviceId device_id;
    /**
     * @brief Идентификатор выполняемой операции.
     */
    std::string operation_id;
    /**
     * @brief Список аргументов операции.
     */
    std::vector<Value> arguments;

public:
    /**
     * @brief Конструктор объекта команды.
     *
     * Описание параметров по позиции в прозе:
     * Первый параметр задает идентификатор целевого устройства (DeviceId).
     * Второй параметр задает строковый идентификатор операции (std::string).
     * Третий параметр задает список аргументов операции (std::vector<Value>).
     */
    Command(DeviceId, std::string, std::vector<Value>);

    /**
     * @brief Получает идентификатор целевого устройства.
     * @return Идентификатор устройства.
     */
    DeviceId getDeviceId() const;
    /**
     * @brief Получает идентификатор выполняемой операции.
     * @return Константная ссылка на строку с идентификатором операции.
     */
    const std::string& getOperationId() const;
    /**
     * @brief Получает список аргументов операции.
     * @return Константная ссылка на вектор аргументов операции.
     */
    const std::vector<Value>& getArguments() const;
};

} // namespace smart_home
