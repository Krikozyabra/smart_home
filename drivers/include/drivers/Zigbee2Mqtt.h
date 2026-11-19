#pragma once

#include "drivers/DeviceCreator.h"
#include "mqtt/MqttTypes.h"

#include <list>
#include <string>

namespace smart_home::mqtt {
class IMqttClient;
}

namespace smart_home {

/**
 * @brief Связывает драйверы Zigbee с MQTT-очередью и JSON-телеметрией.
 * @details Вызывающий код владеет подключением, подпиской на темы устройств
 * и маршрутизацией входящих сообщений. Он сериализует обращения к этому
 * объекту, созданным драйверам и их цифровым двойникам.
 */
class Zigbee2Mqtt {
  public:
    /**
     * @brief Получает MQTT-клиент без передачи владения.
     * @param client Клиент, который должен жить дольше адаптера.
     */
    explicit Zigbee2Mqtt(mqtt::IMqttClient &client) noexcept;
    Zigbee2Mqtt(const Zigbee2Mqtt &) = delete;
    Zigbee2Mqtt &operator=(const Zigbee2Mqtt &) = delete;

    /**
     * @brief Возвращает создателя для протокола zigbee.
     * @details Создание драйвера не требует соединения и не работает с сетью.
     * Адаптер должен жить дольше создателя и всех созданных им драйверов.
     */
    DeviceCreator creator();

    /**
     * @brief Обновляет свойства из сообщения темы устройства.
     * @param driver Драйвер, созданный этим адаптером.
     * @param message MQTT-тема и JSON-полезная нагрузка.
     * @return true, если принято хотя бы одно поле поддерживаемой возможности.
     * @details Ошибочный JSON и значения неизвестных либо недоступных
     * возможностей пропускаются. Значимые поля одного сообщения независимы.
     */
    bool applyTelemetry(IDeviceDriver &driver,
                        const mqtt::MqttMessage &message) const;

  private:
    void publish(const std::string &topic, const std::string &capability_id,
                 const Value &value);

    mqtt::IMqttClient &client_;
    std::list<mqtt::MqttDeliveryToken> pending_;
};

} // namespace smart_home
