#pragma once

#include "cli/ICliOutputSink.h"

#include <iosfwd>

namespace smart_home::cli {

/// @brief Передаёт обычный вывод и ошибки в указанные потоки.
class StreamCliSink final : public ICliOutputSink {
  public:
    /// @brief Заимствует потоки, которые должны пережить приёмник.
    StreamCliSink(std::ostream &out, std::ostream &err) noexcept;

    /** @copydoc ICliOutputSink::writeLine */
    void writeLine(std::string_view line) override;
    /** @copydoc ICliOutputSink::writeError */
    void writeError(std::string_view error) override;

  private:
    std::ostream &out_;
    std::ostream &err_;
};

} // namespace smart_home::cli
