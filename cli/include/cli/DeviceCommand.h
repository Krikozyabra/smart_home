#pragma once

#include "cli/DeviceOperation.h"
#include "cli/ICliCommand.h"
#include "common/DeviceId.h"

#include <functional>

namespace smart_home::cli {

/// @brief Команда `device <id> <operation> [arguments]`.
class DeviceCommand final : public ICliCommand {
  public:
    /** @brief Обработчик получает локальный ID и проверенную операцию.
     * @details Операция и приёмник вывода заимствуются на время вызова.
     */
    using Handler = std::function<void(DeviceId, const DeviceOperation &,
                                       ICliOutputSink &)>;

    /** @brief Принимает во владение обработчик.
     * @throws std::invalid_argument При пустом обработчике.
     */
    explicit DeviceCommand(Handler handler);

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
