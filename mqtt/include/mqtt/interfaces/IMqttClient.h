/** @file IMqttClient.h
 * @brief Независимый от Mosquitto интерфейс чтения и записи MQTT.
 */
#pragma once

#include "mqtt/MqttConfig.h"
#include "mqtt/MqttErrors.h"
#include "mqtt/MqttTypes.h"

#include <chrono>
#include <string_view>

namespace smart_home::mqtt {

/** @brief Клиент MQTT с очередью входящих сообщений и отслеживанием подтверждений.
 * @details Методы потокобезопасны. connect/disconnect и отправка запросов сериализованы.
 * Чтение и ожидание подтверждений можно выполнять в других потоках. Перед уничтожением
 * объекта вызывающий код должен завершить собственные потоки, использующие этот объект.
 */
class IMqttClient {
public:
    /** @brief Освобождает интерфейс. */
    virtual ~IMqttClient() = default;
    /** @brief Подключается и ожидает успешный CONNACK.
     * @param config Настройки брокера.
     * @throws std::invalid_argument Некорректная конфигурация.
     * @throws std::logic_error Клиент уже запущен.
     * @throws MqttConnectionException Отказ брокера.
     * @throws MqttException Ошибка библиотеки либо тайм-аут.
     * @details connectTimeout ограничивает ожидание CONNACK, а не системное разрешение DNS.
     */
    virtual void connect(const MqttConfig& config) = 0;
    /** @brief Останавливает переподключение, будит читателей и присоединяет сетевой поток.
     * @details Повторный вызов допустим. Неподтверждённые операции отменяются; очередь
     * можно дочитать. Системные DNS/TLS операции могут увеличить время завершения.
     */
    virtual void disconnect() noexcept = 0;
    /** @brief Проверяет подключение. @return true после успешного CONNACK. */
    virtual bool isConnected() const noexcept = 0;
    /** @brief Получает состояние. @return Текущее состояние подключения. */
    virtual MqttConnectionState state() const noexcept = 0;
    /** @brief Отправляет запрос публикации, не дожидаясь подтверждения.
     * @param topic Непустая тема без NUL и подстановок.
     * @param payload Произвольные байты; библиотека копирует их до возврата.
     * @param qos Уровень QoS 0, 1 или 2.
     * @param retain Сохранить сообщение на брокере.
     * @return Собственный токен для waitForOperation или waitForDelivery.
     * @throws std::invalid_argument Некорректная тема, QoS или размер.
     * @throws MqttException Нет соединения, превышен лимит или ошибка библиотеки.
     */
    virtual MqttDeliveryToken publish(std::string_view topic,
        const std::vector<std::uint8_t>& payload,
        MqttQoS qos = MqttQoS::AtMostOnce, bool retain = false) = 0;
    /** @brief Запрашивает подписку; успешный возврат не означает успешный SUBACK.
     * @param topicFilter Фильтр; допускаются корректные '+' и '#'.
     * @param qos Запрошенный максимум QoS.
     * @return Токен SUBACK; результат содержит фактически разрешённый QoS.
     * @throws std::invalid_argument Некорректный фильтр или QoS.
     * @throws MqttException Нет соединения, превышен лимит или ошибка библиотеки.
     */
    virtual MqttDeliveryToken subscribe(std::string_view topicFilter,
        MqttQoS qos = MqttQoS::AtMostOnce) = 0;
    /** @brief Запрашивает отписку; требуется активное соединение.
     * @param topicFilter Фильтр подписки.
     * @return Токен UNSUBACK.
     * @throws std::invalid_argument Некорректный фильтр.
     * @throws MqttException Нет соединения, превышен лимит или ошибка библиотеки.
     */
    virtual MqttDeliveryToken unsubscribe(std::string_view topicFilter) = 0;
    /** @brief Читает сообщение без ожидания.
     * @param[out] outMessage Сообщение; при false не изменяется.
     * @return true при наличии сообщения.
     */
    virtual bool tryRead(MqttMessage& outMessage) = 0;
    /** @brief Читает сообщение с ограничением времени.
     * @param timeout Неотрицательная длительность ожидания.
     * @param[out] outMessage Сообщение; при false не изменяется.
     * @return true при получении; false после тайм-аута или остановки.
     * @throws std::invalid_argument Отрицательный тайм-аут.
     */
    virtual bool readFor(std::chrono::milliseconds timeout, MqttMessage& outMessage) = 0;
    /** @brief Ожидает результат операции; терминальный результат читается один раз.
     * @param token Токен этой реализации клиента.
     * @param timeout Неотрицательная длительность ожидания.
     * @return Результат; Pending при тайм-ауте сохраняет токен для повторного ожидания.
     * @throws std::invalid_argument Отрицательный тайм-аут.
     * @details Для публикации QoS 0 Completed означает отправку, QoS 1 — PUBACK,
     * QoS 2 — PUBCOMP. Это не подтверждение обработки сообщения подписчиком.
     */
    virtual MqttOperationResult waitForOperation(MqttDeliveryToken token,
        std::chrono::milliseconds timeout) = 0;
    /** @brief Ожидает завершение публикации и читает результат один раз.
     * @param token Токен публикации.
     * @param timeout Неотрицательная длительность ожидания.
     * @return true для Completed; false для остальных результатов.
     * @throws std::invalid_argument Отрицательный тайм-аут.
     */
    virtual bool waitForDelivery(MqttDeliveryToken token,
        std::chrono::milliseconds timeout) = 0;
    /** @brief Возвращает количество потерянных входящих сообщений.
     * @return Счётчик переполнений, больших сообщений и ошибок копирования после connect.
     */
    virtual std::uint64_t droppedMessages() const noexcept = 0;
};

} // namespace smart_home::mqtt
