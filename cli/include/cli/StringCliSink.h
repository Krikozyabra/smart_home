#pragma once

#include "cli/ICliOutputSink.h"

#include <string>

namespace smart_home::cli {

/// @brief Накапливает обычный вывод и ошибки в отдельных строках в памяти.
class StringCliSink final : public ICliOutputSink {
  public:
    /** @copydoc ICliOutputSink::writeLine */
    void writeLine(std::string_view line) override;
    /** @copydoc ICliOutputSink::writeError */
    void writeError(std::string_view error) override;

    /// @brief Возвращает копию обычного вывода.
    std::string output() const;
    /// @brief Возвращает копию сообщений об ошибках.
    std::string error() const;
    /// @brief Очищает оба накопленных текста.
    void clear() noexcept;

  private:
    std::string out_;
    std::string err_;
};

} // namespace smart_home::cli
