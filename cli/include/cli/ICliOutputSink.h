#pragma once

#include <string_view>

namespace smart_home::cli {

/** @brief Приёмник строк ответа и диагностики команды.
 * @details Переданные строки заимствуются на время вызова. Приёмник копирует
 * текст, если хранит его после возврата. Вызывающий сериализует обращения.
 */
class ICliOutputSink {
  public:
    virtual ~ICliOutputSink() = default;

    /// @brief Записывает строку обычного вывода с завершающим переводом строки.
    virtual void writeLine(std::string_view line) = 0;

    /// @brief Записывает сообщение об ошибке с завершающим переводом строки.
    virtual void writeError(std::string_view error) = 0;
};

} // namespace smart_home::cli
