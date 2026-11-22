/**
 * @file IoEvents.h
 * @brief Маски готовности и обработчики событий файловых дескрипторов.
 */
#pragma once

#include <functional>

namespace smart_home::events {

/** @brief Интерес к событиям дескриптора и результат его опроса. */
enum class IoEvent : unsigned {
    None = 0,   ///< Пустая маска. При регистрации отслеживаются обрыв и ошибки.
    Read = 1,   ///< Готовность к чтению.
    Write = 2,  ///< Готовность к записи.
    HangUp = 4, ///< Обрыв соединения. Может сочетаться с готовностью к чтению.
    Error = 8,  ///< Системная ошибка или недействительный дескриптор.
};

/** @return Объединение битов двух масок. */
constexpr IoEvent operator|(IoEvent left, IoEvent right) noexcept {
    return static_cast<IoEvent>(static_cast<unsigned>(left) |
                                static_cast<unsigned>(right));
}

/** @return Пересечение двух масок. */
constexpr IoEvent operator&(IoEvent left, IoEvent right) noexcept {
    return static_cast<IoEvent>(static_cast<unsigned>(left) &
                                static_cast<unsigned>(right));
}

/** @brief Копируемый обработчик заимствованного дескриптора и маски готовности.
 */
using IoHandler = std::function<void(int fd, IoEvent events)>;

} // namespace smart_home::events
