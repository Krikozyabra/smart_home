/** @file OntologyConfig.h
 * @brief Конфигурация и проверка параметров RAM-хранилища.
 */
#pragma once
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
namespace smart_home::ontology {
/** @brief Параметры постоянной базы, рабочего каталога и синхронизации. */
struct OntologyConfig {
    std::string persistent_path = "data/home_ontology.lbdb"; ///< Постоянный файл.
    std::string ram_workspace_base = "/dev/shm"; ///< Родитель закрытого рабочего каталога.
    std::chrono::seconds sync_interval{30}; ///< Период; ноль отключает фоновую синхронизацию.
    std::size_t max_buffer_pool_bytes = 64 * 1024 * 1024; ///< Буфер движка, байты.
    std::size_t max_threads = 2; ///< Число потоков движка.
    bool allow_disk_fallback = false; ///< Разрешить рабочую копию на диске при недоступности RAM.
};
/** @brief Проверяет параметры без изменения файлов.
 * @param config Проверяемые параметры.
 * @throws std::invalid_argument Пустой путь, отрицательный период или недопустимый лимит.
 */
void validateConfig(const OntologyConfig& config);
/** @brief Разбирает неотрицательный целый период в секундах.
 * @param value Текстовое число. @return Период.
 * @throws std::invalid_argument Некорректное число или переполнение.
 */
std::chrono::seconds parseSyncInterval(std::string_view value);
/** @brief Читает SMART_HOME_ONTOLOGY_PATH, _RAM_DIR, _SYNC_SECONDS и _ALLOW_DISK_FALLBACK.
 * @return Проверенная конфигурация; отсутствующие переменные оставляют значения по умолчанию.
 * @throws std::invalid_argument Некорректная переменная окружения.
 */
OntologyConfig configFromEnvironment();
} // namespace smart_home::ontology
