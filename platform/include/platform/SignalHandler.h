/**
 * @file SignalHandler.h
 * @brief Преобразование сигналов ОС в запрос завершения и событие poll().
 */
#pragma once

#include <initializer_list>
#include <vector>

namespace smart_home::platform {

class EventPipe;

/**
 * @brief Устанавливает обработчики сигналов и восстанавливает их при
 * разрушении.
 * @details В процессе допускается один активный экземпляр. Установка и
 *          перемещение выполняются в основном потоке. Установите обработчики до
 *          запуска рабочих потоков и завершите эти потоки до их восстановления.
 *          Другой код не меняет диспозиции зарегистрированных сигналов.
 *          Канал и его дескрипторы остаются открытыми и на месте до разрушения
 *          обработчика. Перемещённый обработчик становится неактивным.
 */
class SignalHandler {
  public:
    /**
     * @brief Регистрирует SIGINT и SIGTERM.
     * @param pipe Заимствованный канал, выделенный для сигналов.
     * @throws std::invalid_argument Пустой канал.
     * @throws std::logic_error В процессе уже установлен SignalHandler.
     * @throws std::system_error Ошибка sigaction().
     */
    explicit SignalHandler(const EventPipe &pipe);

    /**
     * @brief Регистрирует указанные сигналы. При ошибке отменяет частичную
     * установку.
     * @param pipe Заимствованный канал, выделенный для сигналов.
     * @param signals Непустой список номеров сигналов без повторений.
     * @throws std::invalid_argument Пустой канал, пустой список или повтор
     * сигнала.
     * @throws std::logic_error В процессе уже установлен SignalHandler.
     * @throws std::system_error Ошибка sigaction(), например для SIGKILL.
     */
    SignalHandler(const EventPipe &pipe, std::initializer_list<int> signals);

    /** @brief Восстанавливает предыдущие диспозиции сигналов без исключений. */
    ~SignalHandler() noexcept;

    SignalHandler(const SignalHandler &) = delete;
    SignalHandler &operator=(const SignalHandler &) = delete;
    /** @brief Передаёт владение активной регистрацией сигналов. */
    SignalHandler(SignalHandler &&other) noexcept;
    /** @brief Восстанавливает прежние диспозиции и принимает регистрацию
     * другого объекта. */
    SignalHandler &operator=(SignalHandler &&other) noexcept;

    /** @return Признак запроса завершения. Можно читать из любого потока. */
    bool isStopRequested() const noexcept;

    /**
     * @brief Выставляет флаг завершения и пробуждает читателя из любого потока.
     * @return 0 при успехе, EBADF для неактивного объекта или код ошибки
     * записи.
     */
    int requestStop() const noexcept;

    /** @return Заимствованный дескриптор чтения канала, или -1 для неактивного
     * объекта. */
    int eventFd() const noexcept;

    /**
     * @brief Считывает уведомления из канала в потоке-читателе, сохраняя флаг
     * остановки.
     * @throws std::logic_error Объект неактивен.
     * @throws std::runtime_error Ошибка чтения канала.
     */
    void drain() const;

  private:
    struct Disposition;
    void restore() noexcept;

    std::vector<Disposition> previous_;
    const EventPipe *pipe_ = nullptr;
};

} // namespace smart_home::platform
