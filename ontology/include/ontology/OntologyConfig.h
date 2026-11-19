/** @file OntologyConfig.h
 * @brief Конфигурация и проверка параметров RAM-хранилища.
 */
#pragma once
#include <cstddef>
#include <string>
namespace smart_home::ontology {
/** @brief Параметры исходной базы, рабочего каталога и финального снимка. */
struct OntologyConfig {
    std::string database_path = "data/home_ontology.lbdb"; ///< Исходная база; отсутствующий файл создаётся только в RAM.
    std::string dump_path = {}; ///< Финальный снимок при close(); пустой путь отключает автодамп.
    std::string ram_workspace_base = "/dev/shm"; ///< Родитель закрытого рабочего каталога.
    std::size_t max_buffer_pool_bytes = 64 * 1024 * 1024; ///< Буфер движка, байты.
    std::size_t max_threads = 2; ///< Число потоков движка.
    bool allow_disk_fallback = false; ///< Разрешить рабочую копию на диске при недоступности RAM.
};
/** @brief Проверяет параметры без изменения файлов.
 * @param config Проверяемые параметры.
 * @throws std::invalid_argument Пустой обязательный путь, NUL в пути или недопустимый лимит.
 */
void validateConfig(const OntologyConfig& config);
/** @brief Читает SMART_HOME_ONTOLOGY_PATH (database_path и dump_path), _RAM_DIR и _ALLOW_DISK_FALLBACK.
 * @return Проверенная конфигурация; отсутствующие переменные оставляют значения по умолчанию.
 * @throws std::invalid_argument Некорректная переменная окружения.
 */
OntologyConfig configFromEnvironment();
} // namespace smart_home::ontology
