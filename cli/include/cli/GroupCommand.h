#pragma once

#include "cli/DeviceOperation.h"
#include "cli/ICliCommand.h"

#include <functional>

namespace smart_home::cli {

/// @brief Команда `group <room> <type> <operation> [arguments]`.
class GroupCommand final : public ICliCommand {
  public:
    /** @brief Обработчик получает ID комнаты, категорию и проверенную операцию.
     * @details Текст, операция и приёмник вывода заимствуются на время вызова.
     * Комната и подходящие устройства определяются обработчиком.
     */
    using Handler =
        std::function<void(std::string_view room, std::string_view category,
                           const DeviceOperation &, ICliOutputSink &)>;

    /** @brief Принимает во владение обработчик.
     * @throws std::invalid_argument При пустом обработчике.
     */
    explicit GroupCommand(Handler handler);

    /** @copydoc ICliCommand::name */
    std::string_view name() const noexcept override;
    /** @copydoc ICliCommand::validate */
    bool validate(const CliArguments &args, std::string &error) const override;
    /** @copydoc ICliCommand::execute */
    void execute(const CliArguments &args, ICliOutputSink &sink) override;

  private:
    Handler handler_;
};

} // namespace smart_home::cli
