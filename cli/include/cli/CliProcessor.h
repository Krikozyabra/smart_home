#pragma once

#include "cli/ICliCommand.h"

#include <memory>
#include <unordered_map>

namespace smart_home::cli {

/// @brief Результат обработки строки ввода.
enum class CliStatus {
    Executed,        ///< Обработчик вызван и завершился.
    EmptyInput,      ///< Строка не содержит токенов.
    ValidationError, ///< Аргументы отклонены командой.
    CommandNotFound  ///< Имя команды отсутствует в реестре.
};

/** @brief Владеет командами и ищет их по первому токену строки.
 * @details Вызывающий сериализует регистрацию и обработку строк.
 */
class CliProcessor {
  public:
    CliProcessor() = default;
    CliProcessor(const CliProcessor &) = delete;
    CliProcessor &operator=(const CliProcessor &) = delete;
    CliProcessor(CliProcessor &&) noexcept = default;
    CliProcessor &operator=(CliProcessor &&) noexcept = default;

    /** @brief Принимает во владение команду и регистрирует её по имени.
     * @throws std::invalid_argument При nullptr, повторном имени или имени,
     * которое не является одним непустым токеном.
     */
    void registerCommand(std::unique_ptr<ICliCommand> command);

    /** @brief Проверяет и выполняет команду из строки.
     * @param line Заимствованный текст, токены разделяются пробельными
     * ASCII-символами.
     * @param sink Заимствованный приёмник результатов и ошибок.
     * @return Результат обработки. Executed означает, что обработчик
     * завершился.
     * @details Ошибки ввода передаются в sink. Исключения обработчика и
     * приёмника передаются вызывающему коду.
     */
    CliStatus processLine(std::string_view line, ICliOutputSink &sink);

  private:
    std::unordered_map<std::string, std::unique_ptr<ICliCommand>> commands_;
};

} // namespace smart_home::cli
