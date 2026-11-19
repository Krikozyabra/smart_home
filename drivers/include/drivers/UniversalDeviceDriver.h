#pragma once

#include "drivers/DeviceDriverBase.h"
#include "storage/DeviceRecord.h"

#include <functional>
#include <map>

namespace smart_home {

/**
 * @brief Принимает проверенное значение для отправки в протокол.
 * @details Параметры действительны на время вызова. Обработчик не ожидает
 * подтверждения от оборудования, не вызывает этот же драйвер повторно и
 * передаёт ошибку отправки вызывающему коду.
 */
using ActionHandler =
    std::function<void(const std::string &capability_id, const Value &value)>;

/**
 * @brief Владеет универсальным Device и дескриптором с изменяемым набором
 * возможностей.
 * @details Вызывающий код сериализует доступ к драйверу и его Device.
 */
class UniversalDeviceDriver final : public DeviceDriverBase {
  public:
    /**
     * @brief Создаёт базовый драйвер по записи инвентаря.
     * @param record Данные инвентаря, копируемые в устройство и дескриптор.
     * @param transport Обработчик протокола для записываемых возможностей.
     */
    explicit UniversalDeviceDriver(const DeviceRecord &record,
                                   ActionHandler transport = nullptr);

    /**
     * @brief Принимает владение устройством и создаёт пустой дескриптор.
     * @throws std::invalid_argument Устройство отсутствует или его ID
     * отличается от record.
     */
    UniversalDeviceDriver(std::unique_ptr<Device> device,
                          const DeviceRecord &record,
                          ActionHandler transport = nullptr);

    /**
     * @brief Добавляет операции и инициализирует свойство из каталога.
     * @param capability_id ID возможности из CapabilityCatalog.
     * @param handler Переопределение транспорта для операции записи.
     * @details Начальное значение применяется при отсутствии свойства. Ссылки
     * на элементы списка возможностей могут стать недействительными.
     * @throws std::invalid_argument Неизвестная или повторная возможность,
     * обработчик для возможности чтения либо неверный тип существующего
     * свойства.
     */
    void addCapability(const std::string &capability_id,
                       ActionHandler handler = nullptr);

    /**
     * @brief Передаёт запись обработчику или изменяет свойство в памяти.
     * @details Для транспорта состояние обновляет входящая телеметрия.
     * @throws std::invalid_argument Операция не поддерживается или значение
     * неверно.
     */
    void writeCapability(const std::string &capability_id,
                         const Value &value) override;

  private:
    ActionHandler transport_;
    std::map<std::string, ActionHandler> actions_;
};

} // namespace smart_home
