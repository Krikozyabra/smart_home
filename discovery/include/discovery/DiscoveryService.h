/**
 * @file DiscoveryService.h
 * @brief Сервис обнаружения устройств в системе умного дома.
 */

#pragma once

#include "device_management/DeviceManager.h"
#include "discovery/entities/DiscoveryReport.h"
#include "discovery/interfaces/IDeviceDiscoverySource.h"

#include <memory>
#include <vector>

namespace smart_home {

/**
 * @brief Сервис для поиска и регистрации устройств с использованием различных источников обнаружения.
 */
class DiscoveryService {
  private:
    /**
     * @brief Список источников обнаружения устройств.
     */
    std::vector<std::unique_ptr<IDeviceDiscoverySource>> sources;

    /**
     * @brief Ссылка на менеджер устройств.
     */
    DeviceManager &device_manager;

  public:
    /**
     * @brief Конструктор сервиса обнаружения.
     * @param c_device_manager Ссылка на менеджер устройств.
     * @param c_sources Вектор уникальных указателей на источники обнаружения устройств.
     * @throws std::invalid_argument Если один из источников равен nullptr.
     */
    DiscoveryService(
        DeviceManager &c_device_manager,
        std::vector<std::unique_ptr<IDeviceDiscoverySource>> c_sources);

    /**
     * @brief Выполнить однократное сканирование всех источников обнаружения.
     * @return Отчет о результатах обнаружения устройств.
     * @details Ошибки DiscoveryScanError и UnsupportedDriverError включаются в отчет;
     * остальные исключения передаются вызывающему коду.
     */
    DiscoveryReport scanOnce();
};

} // namespace smart_home
