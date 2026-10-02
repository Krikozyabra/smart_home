/**
 * @file DiscoveryError.h
 * @brief Определение типов ошибок и состояний процесса обнаружения устройств.
 */

#pragma once

#include "discovery/entities/DiscoveredDevice.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>

namespace smart_home {

/**
 * @class DiscoveryScanError
 * @brief Исключение, выбрасываемое при ошибке во время сканирования устройств.
 */
class DiscoveryScanError : public std::runtime_error {
  public:
    /**
     * @brief Конструктор исключения сканирования устройств.
     * @param message Текстовое описание ошибки.
     */
    explicit DiscoveryScanError(const std::string &message)
        : std::runtime_error(message) {}
};

/**
 * @enum DiscoveryState
 * @brief Состояние процесса обнаружения устройств.
 */
enum class DiscoveryState {
    /**
     * @brief Этап сканирования источников обнаружения.
     */
    Scan,
    /**
     * @brief Этап регистрации обнаруженных устройств.
     */
    Registration
};

/**
 * @struct DiscoveryError
 * @brief Информация об ошибке, возникшей в процессе обнаружения устройств.
 */
struct DiscoveryError {
    /**
     * @brief Состояние процесса обнаружения, на котором произошла ошибка.
     */
    DiscoveryState state;

    /**
     * @brief Индекс источника обнаружения, вызвавшего ошибку.
     */
    std::size_t source_index;

    /**
     * @brief Обнаруженное устройство, с которым связана ошибка (если применимо).
     */
    std::optional<DiscoveredDevice> discovered_device;

    /**
     * @brief Текстовое сообщение с описанием ошибки.
     */
    std::string message;
};

} // namespace smart_home
