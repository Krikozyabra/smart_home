/**
 * @file DiscoveryReport.h
 * @brief Определение структуры отчета о результатах обнаружения устройств.
 */

#pragma once

#include "discovery/entities/DiscoveryError.h"

#include <cstddef>
#include <vector>

namespace smart_home {

/**
 * @struct DiscoveryReport
 * @brief Отчет, содержащий статистику и ошибки процесса обнаружения устройств.
 */
struct DiscoveryReport {
    /**
     * @brief Количество источников обнаружения, к которым было выполнено обращение.
     */
    std::size_t sources_attempted = 0;

    /**
     * @brief Количество источников обнаружения, опрос которых завершился успешно.
     */
    std::size_t sources_succeeded = 0;

    /**
     * @brief Общее количество найденных устройств.
     */
    std::size_t devices_found = 0;

    /**
     * @brief Количество успешно обработанных устройств.
     */
    std::size_t devices_processed = 0;

    /**
     * @brief Список ошибок, возникших во время процесса обнаружения.
     */
    std::vector<DiscoveryError> errors{};
};

} // namespace smart_home
