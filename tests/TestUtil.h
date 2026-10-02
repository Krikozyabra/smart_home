/**
 * @file TestUtil.h
 * @brief Вспомогательные утилиты для модульного тестирования.
 */

#pragma once

#include <stdexcept>
#include <string>

/**
 * @brief Проверяет выполнение условия и выбрасывает исключение при его нарушении.
 * @param condition Проверяемое логическое условие.
 * @param message Сообщение об ошибке, передаваемое в исключение.
 * @throws std::runtime_error Если condition ложно.
 */
inline void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}

/**
 * @brief Проверяет, что выполнение переданной функции приводит к выбросу ожидаемого исключения.
 * @tparam Exception Тип ожидаемого исключения.
 * @tparam Function Тип вызываемого функционального объекта.
 * @param function Функция или вызываемый объект для проверки.
 * @throws std::runtime_error Если ожидаемое исключение типа Exception не было выброшено.
 */
template <typename Exception, typename Function>
void expectException(Function function) {
    try {
        function();
    } catch (const Exception &) {
        return;
    }

    throw std::runtime_error("Expected exception was not thrown");
}
