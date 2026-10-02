#pragma once

/**
 * @file CommandDispatcher.h
 * @brief Диспетчер команд для устройств умного дома.
 */

#include "descriptors/DeviceDescriptor.h"
#include "descriptors/OperationDescriptor.h"
#include "descriptors/ParameterDescriptor.h"
#include "command/Command.h"
#include "command/Value.h"
#include "devices/Device.h"
#include "registry/DeviceRegistry.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace smart_home {

/**
 * @class CommandDispatcher
 * @brief Диспетчер команд для маршрутизации и выполнения операций над устройствами.
 */
class CommandDispatcher {
  private:
    /**
     * @brief Псевдоним типа указателя на функцию-обработчик операции устройства.
     */
    using Handler = std::optional<Value> (*)(Device &,
                                             const OperationDescriptor &,
                                             const Command &command);

    /**
     * @brief Ссылка на реестр зарегистрированных устройств.
     */
    DeviceRegistry &registry;

    /**
     * @brief Таблица соответствия идентификаторов операций их обработчикам.
     */
    std::unordered_map<std::string, Handler> handlers;

    /**
     * @brief Выполняет поиск дескриптора операции по ее идентификатору.
     * @param descriptor Дескриптор устройства, в котором выполняется поиск.
     * @param operation_id Идентификатор искомой операции.
     * @return Указатель на найденный OperationDescriptor или nullptr, если операция не найдена.
     */
    const OperationDescriptor *
    findOperation(const DeviceDescriptor &descriptor,
                  const std::string &operation_id) const;

    /**
     * @brief Проверяет корректность операции записи с единственным входным параметром.
     * @param operation Дескриптор проверяемой операции.
     * @param command Объект команды с переданными параметрами.
     * @return Константная ссылка на дескриптор валидированного параметра ParameterDescriptor.
     */
    static const ParameterDescriptor &
    validateSingleInputWrite(const OperationDescriptor &operation,
                             const Command &command);

    /**
     * @brief Проверяет корректность операции чтения без входных параметров.
     * @param operation Дескриптор проверяемой операции.
     * @param command Объект команды.
     * @return Константная ссылка на дескриптор возвращаемого параметра ParameterDescriptor.
     */
    static const ParameterDescriptor &
    validateNoInputRead(const OperationDescriptor &operation,
                        const Command &command);

    /**
     * @brief Выполняет операцию установки уровня яркости устройства.
     * @param device Устройство, для которого устанавливается яркость.
     * @param operation Дескриптор операции установки яркости.
     * @param command Команда, содержащая значение устанавливаемой яркости.
     * @return Опциональное значение Value с результатом выполнения команды.
     */
    static std::optional<Value>
    executeBrightnessSet(Device &device, const OperationDescriptor &operation,
                         const Command &command);

    /**
     * @brief Выполняет операцию получения текущей яркости устройства.
     * @param device Устройство, у которого запрашивается яркость.
     * @param operation Дескриптор операции получения яркости.
     * @param command Команда запроса яркости.
     * @return Значение яркости в виде std::optional<Value>.
     */
    static std::optional<Value>
    executeBrightnessGet(Device &device, const OperationDescriptor &operation,
                         const Command &command);

    /**
     * @brief Выполняет операцию получения текущей температуры с датчика.
     * @param device Устройство-датчик температуры.
     * @param operation Дескриптор операции получения температуры.
     * @param command Команда запроса температуры.
     * @return Значение температуры в виде std::optional<Value>.
     */
    static std::optional<Value>
    executeTemperatureGet(Device &device, const OperationDescriptor &operation,
                          const Command &command);

    /**
     * @brief Выполняет операцию получения состояния включения/выключения устройства.
     * @param device Опрашиваемое устройство.
     * @param operation Дескриптор операции получения состояния.
     * @param command Команда запроса состояния.
     * @return Текущее состояние включения в виде std::optional<Value>.
     */
    static std::optional<Value>
    executeOnOffGet(Device &device, const OperationDescriptor &operation,
                    const Command &command);

    /**
     * @brief Выполняет операцию включения или выключения устройства.
     * @param device Целевое устройство.
     * @param operation Дескриптор операции включения/выключения.
     * @param command Команда с новым состоянием включения/выключения.
     * @return Опциональное значение Value с результатом выполнения команды.
     */
    static std::optional<Value>
    executeOnOffSet(Device &device, const OperationDescriptor &operation,
                    const Command &command);

    /**
     * @brief Выполняет операцию получения текущего цвета устройства.
     * @param device Устройство, у которого запрашивается цвет.
     * @param operation Дескриптор операции получения цвета.
     * @param command Команда запроса цвета.
     * @return Текущий цвет устройства в виде std::optional<Value>.
     */
    static std::optional<Value>
    executeColorGet(Device &device, const OperationDescriptor &operation,
                    const Command &command);

    /**
     * @brief Выполняет операцию установки цвета устройства.
     * @param device Устройство, цвет которого изменяется.
     * @param operation Дескриптор операции установки цвета.
     * @param command Команда со значением нового цвета.
     * @return Опциональное значение Value с результатом выполнения команды.
     */
    static std::optional<Value>
    executeColorSet(Device &device, const OperationDescriptor &operation,
                    const Command &command);

  public:
    /**
     * @brief Конструктор диспетчера команд.
     * @param registry Ссылка на реестр устройств DeviceRegistry.
     */
    explicit CommandDispatcher(DeviceRegistry &registry);

    /**
     * @brief Выполняет переданную команду для целевого устройства.
     * @param command Команда для исполнения.
     * @return Значение операции чтения; std::nullopt для операции записи.
     * @throws std::runtime_error Если целевое устройство не зарегистрировано.
     * @throws std::invalid_argument Если операция не найдена или аргументы некорректны.
     * @throws std::logic_error Если обработчик отсутствует или дескриптор несовместим с устройством.
     */
    std::optional<Value> execute(const Command &command);
};

} // namespace smart_home
