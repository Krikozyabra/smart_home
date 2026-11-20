/**
 * @file SystemError.h
 * @brief Ошибки системных вызовов Linux и повтор при EINTR.
 */
#pragma once

#include <cerrno>
#include <string_view>

namespace smart_home::platform {

/**
 * @brief Выбрасывает исключение с именем операции и системным сообщением.
 * @param operation Имя системного вызова или описание операции.
 * @param error Код ошибки, по умолчанию текущее значение errno.
 * @throws std::system_error Ошибка в категории std::generic_category().
 */
[[noreturn]] void throwSystemError(std::string_view operation,
                                   int error = errno);

/**
 * @brief Повторяет вызов, пока он возвращает -1 с errno == EINTR.
 * @param operation Вызываемый объект без аргументов с целочисленным
 * результатом. Его можно вызвать повторно.
 * @return Результат последней попытки. При ошибке errno остаётся доступным.
 * @note Для close() на Linux повтор недопустим: номер уже освобождён.
 */
template <class Operation>
auto retryInterrupted(Operation &&operation) noexcept(noexcept(operation())) {
    auto result = operation();
    while (result == -1 && errno == EINTR)
        result = operation();
    return result;
}

} // namespace smart_home::platform
