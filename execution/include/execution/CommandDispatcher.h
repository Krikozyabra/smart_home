#pragma once

#include "command/Command.h"
#include "registry/DeviceRegistry.h"

#include <optional>
#include <string>

namespace smart_home {

/** @brief Выполняет зарегистрированные операции возможностей устройств. */
class CommandDispatcher {
  public:
    /**
     * @brief Получает реестр активных драйверов без передачи владения.
     * @param registry Реестр, который должен жить дольше диспетчера.
     */
    explicit CommandDispatcher(DeviceRegistry &registry);

    /**
     * @brief Проверяет команду по дескриптору и выполняет операцию.
     * @return Значение цифрового двойника при чтении или std::nullopt при
     * записи.
     * @throws std::runtime_error Устройство отсутствует или отправка
     * завершилась ошибкой.
     * @throws std::invalid_argument Операция отсутствует или аргументы неверны.
     * @throws std::logic_error Дескриптор или свойство несовместимы с
     * каталогом.
     * @details Вызывающий код сериализует доступ к реестру и драйверам.
     */
    std::optional<Value> execute(const Command &command);

  private:
    static const ParameterDescriptor &
    validateSingleInputWrite(const OperationDescriptor &operation,
                             const Command &command);
    static const ParameterDescriptor &
    validateNoInputRead(const OperationDescriptor &operation,
                        const Command &command);
    static std::optional<Value>
    executeOperation(IDeviceDriver &driver, const std::string &capability_id,
                     const OperationDescriptor &operation,
                     const Command &command);

    DeviceRegistry &registry;
};

} // namespace smart_home
