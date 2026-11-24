#pragma once

#include "cli/ICliCommand.h"

#include <cstddef>
#include <functional>

namespace smart_home::cli {

/// @brief Команда с фиксированным числом аргументов.
class ActionCommand final : public ICliCommand {
  public:
    /// @brief Обработчик получает проверенные токены и приёмник вывода.
    using Handler = std::function<void(const CliArguments &, ICliOutputSink &)>;

    /** @brief Сохраняет имя, число аргументов и обработчик.
     * @param name Имя команды.
     * @param expected_args Требуемое число аргументов.
     * @param handler Обработчик, которым владеет команда.
     * @throws std::invalid_argument При пустом имени или обработчике.
     */
    ActionCommand(std::string name, std::size_t expected_args, Handler handler);

    /** @copydoc ICliCommand::name */
    std::string_view name() const noexcept override;
    /** @copydoc ICliCommand::validate */
    bool validate(const CliArguments &args, std::string &error) const override;
    /** @copydoc ICliCommand::execute */
    void execute(const CliArguments &args, ICliOutputSink &sink) override;

  private:
    std::string name_;
    std::size_t expected_args_;
    Handler handler_;
};

} // namespace smart_home::cli
