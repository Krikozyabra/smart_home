/**
 * @file Reactor.h
 * @brief Однопоточный цикл I/O, монотонных таймеров и межпоточных задач.
 */
#pragma once

#include "events/EventBus.h"
#include "events/IoEvents.h"
#include "events/TimerHandle.h"

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace smart_home::events {

/**
 * @brief Ожидает готовность дескрипторов, таймеров и очереди задач через
 * poll().
 * @details run(), step() и изменения списка дескрипторов и таймеров выполняются
 *          последовательно на потоке реактора. Коллбэки могут менять
 * регистрации и ставить задачи. post(), stop() и isRunning() доступны любому
 * потоку. Вызывающий код завершает обращения к реактору до его уничтожения.
 */
class Reactor {
  public:
    /** @brief Копируемая задача с выполнением на потоке реактора. */
    using Task = std::function<void()>;
    /** @brief Получает исключение обработчика на потоке реактора. */
    using ErrorHandler = std::function<void(std::exception_ptr)>;

    /**
     * @brief Открывает неблокирующий eventfd для пробуждения цикла.
     * @param on_error Необязательный получатель исключений обработчиков. Даже
     *        если он выбрасывает исключение, последующая диспетчеризация
     * доступна.
     * @throws std::system_error Ошибка создания eventfd().
     */
    explicit Reactor(ErrorHandler on_error = {});
    /** @brief Освобождает дескриптор пробуждения и оставшиеся регистрации. */
    ~Reactor() noexcept;

    Reactor(const Reactor &) = delete;
    Reactor &operator=(const Reactor &) = delete;

    /**
     * @brief Регистрирует обработчик заимствованного дескриптора.
     * @param fd Открытый дескриптор. Остаётся открытым до снятия регистрации.
     * @param events Маска интереса. Обрыв и ошибки доставляются при любой
     * маске.
     * @param handler Обработчик, получающий совмещённую маску готовности.
     * @throws std::invalid_argument Некорректный или повторный fd, маска либо
     * пустой обработчик.
     */
    void addHandler(int fd, IoEvent events, IoHandler handler);

    /**
     * @brief Заменяет маску интереса существующего обработчика.
     * @param fd Зарегистрированный дескриптор.
     * @param events Новая маска интереса.
     * @throws std::invalid_argument Неизвестный fd или недопустимая маска.
     */
    void modifyHandler(int fd, IoEvent events);

    /** @brief Удаляет обработчик fd. Повторное снятие безопасно. */
    void removeHandler(int fd) noexcept;

    /**
     * @brief Регистрирует таймер по std::chrono::steady_clock.
     * @param interval Интервал в миллисекундах. Ноль разрешён для разового
     * таймера.
     * @param periodic Повторять через interval после завершения коллбэка.
     * @param callback Обработчик таймера на потоке реактора.
     * @return Токен управления регистрацией. Его уничтожение отменяет таймер.
     * @throws std::invalid_argument Некорректный интервал или пустой
     * обработчик.
     */
    TimerHandle addTimer(std::chrono::milliseconds interval, bool periodic,
                         Task callback);

    /**
     * @brief Ставит задачу в потокобезопасную FIFO-очередь и пробуждает
     * реактор.
     * @param task Задача, передаваемая во владение реактора.
     * @throws std::invalid_argument Передана пустая задача.
     * @throws std::system_error Ошибка пробуждения. Задача не остаётся в
     * очереди.
     * @note Метод использует мьютекс. Вызывайте его из обычных потоков.
     */
    void post(Task task);

    /**
     * @brief Ставит типизированное событие в очередь для публикации на потоке
     * реактора.
     * @param bus Шина-получатель. После уничтожения шины доставка пропускается.
     * @param event Событие, копируемое или перемещаемое в очередь.
     * @details Сам объект bus должен быть действителен во время вызова post().
     */
    template <typename Event> void post(EventBus &bus, Event &&event) {
        using StoredEvent = std::remove_cv_t<std::remove_reference_t<Event>>;
        auto value = std::make_shared<StoredEvent>(std::forward<Event>(event));
        std::weak_ptr<detail::EventBusState> receiver = bus.state_;
        post([receiver = std::move(receiver), value = std::move(value)] {
            if (const auto state = receiver.lock())
                EventBus::publishErased(*state, typeid(StoredEvent),
                                        value.get());
        });
    }

    /**
     * @brief Ожидает и диспетчеризует события до запроса stop().
     * @throws std::logic_error run() или step() уже активен.
     * @throws std::system_error Ошибка системного опроса или чтения eventfd.
     */
    void run();

    /**
     * @brief Выполняет одну итерацию. Ошибки коллбэков передаются on_error.
     * @param timeout_ms Максимальное ожидание в миллисекундах. Ноль проверяет
     *        готовность немедленно. Отрицательное значение допускает ожидание
     * без лимита.
     * @return Число вызванных коллбэков, включая завершившиеся с исключением.
     * @details Таймаут ограничивает ожидание готовности. I/O обрабатывается в
     * порядке регистрации, затем задачи и таймеры, готовые к началу
     * диспетчеризации.
     * @throws std::logic_error run() или step() уже активен.
     * @throws std::system_error Ошибка системного опроса или чтения eventfd.
     */
    int step(int timeout_ms = -1);

    /**
     * @brief Пробуждает и останавливает текущий или ближайший run()/step().
     * @details Запрос учитывается между коллбэками и сбрасывается при выходе
     *          из run()/step(). Оставшиеся задачи доступны следующему запуску.
     */
    void stop() noexcept;

    /** @return Признак активного run() или step() на момент проверки. */
    bool isRunning() const noexcept;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace smart_home::events
