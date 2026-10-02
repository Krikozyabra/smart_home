/**
 * @file SimulatedDiscoverySource.h
 * @brief Имитационный источник обнаружения устройств.
 */

#pragma once

#include "discovery/entities/DiscoveredDevice.h"
#include "discovery/interfaces/IDeviceDiscoverySource.h"

#include <string_view>
#include <vector>

namespace smart_home {

/**
 * @brief Класс имитационного источника обнаружения устройств для тестирования.
 */
class SimulatedDiscoverySource : public IDeviceDiscoverySource {
  private:
    /**
     * @brief Кэш обнаруженных устройств.
     */
    std::vector<DiscoveredDevice> cache;

    /**
     * @brief Флаг, имитирующий возникновение ошибки при сканировании.
     */
    bool is_error;

  public:
    /**
     * @brief Конструктор имитационного источника обнаружения устройств.
     * Первый параметр — список обнаруженных устройств для инициализации кэша.
     * Второй параметр — флаг наличия ошибки при сканировании.
     */
    SimulatedDiscoverySource(std::vector<DiscoveredDevice>, bool);

    /**
     * @brief Выполнить сканирование устройств.
     * @return Список обнаруженных устройств.
     * @throws DiscoveryScanError Если включена имитация ошибки сканирования.
     */
    std::vector<DiscoveredDevice> scan() override;

    /**
     * @brief Получить имя класса источника обнаружения.
     * @return Имя класса в виде строкового представления std::string_view.
     */
    std::string_view get_class_name() const override;
};

} // namespace smart_home
