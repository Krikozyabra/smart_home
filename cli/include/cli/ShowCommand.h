#pragma once

#include "cli/ICliCommand.h"
#include "common/DeviceId.h"

#include <functional>

namespace smart_home::cli {

/// @brief Команда `show <id>` для просмотра сведений об устройстве.
class ShowCommand final : public ICliCommand {
  public:
    /// @brief Обработчик получает неотрицательный локальный ID и приёмник
    /// вывода.
    using Handler = std::function<void(DeviceId, ICliOutputSink &)>;

    /** @brief Принимает во владение обработчик.
     * @throws std::invalid_argument При пустом обработчике.
     */
    explicit ShowCommand(Handler handler);

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
