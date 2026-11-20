/**
 * @file DescriptorIO.h
 * @brief Неблокирующее чтение файлового дескриптора Linux.
 */
#pragma once

#include <cstddef>
#include <optional>

namespace smart_home::platform {

/** @brief Номер дескриптора стандартного ввода процесса Linux. */
inline constexpr int standardInputFd = 0;

/**
 * @brief Читает доступные байты, повторяя системный вызов при EINTR.
 * @param fd Заимствованный дескриптор, открытый в неблокирующем режиме.
 * @param buffer Буфер, доступный для записи как минимум size байт.
 * @param size Максимальное число читаемых байт, больше нуля.
 * @return Число байт, 0 при EOF, std::nullopt при EAGAIN или EWOULDBLOCK.
 * @throws std::system_error Прочая ошибка read().
 */
std::optional<std::size_t> readNonBlocking(int fd, void *buffer,
                                           std::size_t size);

} // namespace smart_home::platform
