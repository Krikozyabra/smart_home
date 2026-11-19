/**
 * @file DeviceManager.h
 * @brief Определение менеджера устройств и связанных классов исключений.
 */

#pragma once

#include "ontology/IOntologyStorage.h"
#include "registry/DeviceRegistry.h"
#include "registry/DriverFactoryRegistry.h"
#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace smart_home {

/**
 * @class UnsupportedDriverError
 * @brief Исключение, выбрасываемое при попытке использовать неподдерживаемый драйвер устройства.
 */
class UnsupportedDriverError : public std::invalid_argument {
  public:
    /**
     * @brief Конструктор исключения неподдерживаемого драйвера.
     * @param message Описание ошибки.
     */
    explicit UnsupportedDriverError(const std::string &message)
        : std::invalid_argument(message) {}
};

/**
 * @class DeviceManager
 * @brief Координирует инвентарь, активные драйверы и их представление в онтологии.
 * @details Зависимости передаются без владения и должны жить дольше менеджера.
 * Собственных потоков и блокировок нет: вызывающий код сериализует обращения
 * к менеджеру и зависимостям. Общей транзакции нет; частичные изменения
 * сохраняются для повторной попытки. Менеджер не вызывает dumpToFile() или close().
 */
class DeviceManager {
  private:
    /**
     * @brief Ссылка на хранилище записей об устройствах.
     */
    IDeviceStorage &storage;
    /**
     * @brief Ссылка на реестр фабрик драйверов.
     */
    const DriverFactoryRegistry &factory_registry;
    /**
     * @brief Ссылка на реестр активных устройств.
     */
    DeviceRegistry &device_registry;
    /** @brief Онтология с заранее подготовленным каталогом возможностей. */
    ontology::IOntologyStorage &ontology_storage;

    /**
     * @brief Получает существующую запись устройства или создает новую при ее отсутствии.
     * @param physical_id Физический идентификатор устройства.
     * @param driver_id Идентификатор драйвера устройства.
     * @param default_name Имя устройства по умолчанию.
     * @return Запись об устройстве DeviceRecord.
     */
    DeviceRecord getOrCreate(const std::string &physical_id,
                             const std::string &driver_id,
                             const std::string &default_name);

    /** @brief Дополняет узел и HAS_FEATURE, проверяя имя и лишние связи.
     * @param record Инвентарная запись.
     * @param driver Активный драйвер; определяет ожидаемые возможности.
     * @throws std::runtime_error Несовпадение имени, лишние связи или отсутствие возможности в каталоге.
     */
    void syncDeviceOntology(const DeviceRecord &record,
                            const IDeviceDriver &driver);

  public:
    /**
     * @brief Конструктор менеджера устройств.
     * @param storage Ссылка на хранилище данных об устройствах.
     * @param factory_registry Ссылка на реестр фабрик драйверов устройств.
     * @param device_registry Ссылка на реестр зарегистрированных устройств.
     * @param ontology_storage Ссылка на онтологию с каталогом возможностей.
     * @details Конструктор не изменяет зависимости и не выполняет восстановление.
     */
    DeviceManager(IDeviceStorage &storage,
                  const DriverFactoryRegistry &factory_registry,
                  DeviceRegistry &device_registry,
                  ontology::IOntologyStorage &ontology_storage);

    /**
     * @brief Восстанавливает ранее сохраненные устройства из хранилища.
     * @details Сначала подготавливает все отсутствующие драйверы, затем добавляет
     * их в реестр и дополняет граф для всех записей, включая уже активные.
     * Ошибка подготовки не меняет реестр и граф. Ошибки последующих фаз
     * не откатывают изменения; повторный вызов завершает недостающие операции.
     * Существующие драйверы, их значения и назначения комнат сохраняются.
     * Исключения зависимостей передаются вызывающему коду.
     * @return Число добавленных драйверов после успешной работы с графом;
     * 0 при пустом инвентаре, повторном вызове или дополнении только графа.
     * @throws std::runtime_error Если фабрика сохраненного драйвера не найдена.
     * @throws std::runtime_error Несовпадение имени, лишние связи или отсутствие возможности в каталоге.
     * @throws std::logic_error Если фабрика вернула nullptr.
     */
    std::size_t restoreStoredDevices();

    /**
     * @brief Регистрирует вновь обнаруженное устройство.
     * @param physical_id Физический идентификатор устройства.
     * @param driver_id Идентификатор драйвера устройства.
     * @param default_name Имя устройства по умолчанию.
     * @details Проверяет фабрику до обращения к storage. Повторный вызов
     * сохраняет запись, активный драйвер, его значения и комнату, дополняя граф.
     * При ошибке запись, драйвер и частичные изменения графа не откатываются;
     * после устранения причины вызов можно повторить. Исключения зависимостей
     * передаются вызывающему коду. Постоянное сохранение выполняет runtime.
     * @return Собственная копия инвентарной записи; граф проверен и дополнен.
     * @throws UnsupportedDriverError Если фабрика указанного драйвера не найдена.
     * @throws std::runtime_error Несовпадение имени, лишние связи или отсутствие возможности в каталоге.
     * @throws std::logic_error Если фабрика вернула nullptr.
     */
    DeviceRecord registerDiscoveredDevice(const std::string &physical_id,
                                          const std::string &driver_id,
                                          const std::string &default_name);
};

} // namespace smart_home
