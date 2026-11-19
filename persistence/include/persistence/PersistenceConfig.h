#pragma once

#include <cstddef>
#include <filesystem>

namespace smart_home::persistence {

/** @brief Настройки поколений и рабочих файлов. */
struct PersistenceConfig {
    std::filesystem::path state_dir = "data/runtime"; ///< Каталог поколений.
    std::filesystem::path ram_dir =
        "/dev/shm";           ///< Рабочий каталог на tmpfs/ramfs.
    std::size_t capacity = 4; ///< Число слотов, включая снимки и их результаты.
};

/**
 * @brief Проверяет настройки без открытия ресурсов.
 * @throws std::invalid_argument Пустой путь, NUL в пути или нулевая ёмкость.
 */
void validateConfig(const PersistenceConfig &config);

/**
 * @brief Проверяет существующий рабочий каталог на tmpfs/ramfs.
 * @throws std::runtime_error Каталог отсутствует, недоступен или расположен на
 * диске.
 */
void requireRamDirectory(const std::filesystem::path &path);

} // namespace smart_home::persistence
