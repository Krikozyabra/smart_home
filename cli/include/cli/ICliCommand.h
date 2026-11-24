#pragma once

#include "cli/ICliOutputSink.h"

#include <string>
#include <string_view>
#include <vector>

namespace smart_home::cli {

/// @brief Токены после имени команды. Строки заимствуются на время вызова.
using CliArguments = std::vector<std::string_view>;

/// @brief Команда с отдельной проверкой аргументов и выполнением.
class ICliCommand {
  public:
    virtual ~ICliCommand() = default;

    /** @brief Возвращает имя из одного непустого токена.
     * @return Заимствованное имя, действительное на протяжении жизни команды.
     */
    virtual std::string_view name() const noexcept = 0;

    /** @brief Проверяет аргументы без вызова обработчика.
     * @param args Токены аргументов.
     * @param[out] error Диагностика для пользователя при результате false.
     * @return true, если аргументы корректны.
     */
    virtual bool validate(const CliArguments &args,
                          std::string &error) const = 0;

    /** @brief Вызывает обработчик с указанным приёмником вывода.
     * @pre Аргументы успешно прошли validate().
     * @details Аргументы и приёмник заимствуются на время вызова.
     * Исключения обработчика передаются вызывающему коду.
     */
    virtual void execute(const CliArguments &args, ICliOutputSink &sink) = 0;
};

} // namespace smart_home::cli
