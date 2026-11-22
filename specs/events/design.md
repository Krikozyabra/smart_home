# Технические решения модуля events

Статус: реализовано. Основание — [spec.md](spec.md) и `AGENTS.md`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), C++17.

## 1. Цели и границы модуля

Модуль предоставляет внутрипроцессную событийно-ориентированную инфраструктуру:
- **`EventBus`**: Типобезопасная шина событий без обязательного общего базового класса (неинтрузивный дизайн на базе `std::type_index`).
- **`Subscription`**: RAII-токен владения подпиской с автоматической отпиской при разрушении.
- **`Reactor`**: Мультиплексирование дескрипторов ввода-вывода через `poll(2)`, планирование таймеров по монотонным часам и межпоточная очередь задач с неблокирующим пробуждением (Linux `eventfd`).

Модуль не содержит доменной логики устройств, базы данных или сетевой сериализации (не заменяет внешний MQTT).

## 2. Архитектура и зависимости

- Пространство имён: `smart_home::events`.
- Публичный интерфейс C++17 в каталоге `events/include/events/`.
- Статическая библиотека `smart_home_events` с alias `SmartHome::Events`.
- Зависимости CMake: стандартная библиотека C++17, системные заголовки Linux POSIX (`<poll.h>`, `<sys/eventfd.h>`, `<unistd.h>`). Внешние библиотеки не требуются.

## 3. Детальное проектирование компонентов

### 3.1. Типизированная шина событий (EventBus)
Класс `EventBus` обеспечивает маршрутизацию событий по их C++ типам:
```cpp
class EventBus {
public:
    EventBus();
    ~EventBus();

    EventBus(const EventBus &) = delete;
    EventBus &operator=(const EventBus &) = delete;

    template <typename Event>
    Subscription subscribe(std::function<void(const Event &)> handler);

    template <typename Event>
    void publish(const Event &event);

    std::size_t subscriberCount() const noexcept;
};
```
- **Идентификация типов**: Использование `std::type_index(typeid(Event))`.
- **Стирание типов**: Подписчики хранятся внутри шины в виде `std::function<void(const void *)>`, оборачивающей пользовательский строго типизированный обработчик `std::function<void(const Event &)>`.
- **Реентерабельность**. `publish()` копирует под мьютексом ссылки на записи подписок.
  Обработчики вызываются вне мьютекса. Состояние коллбэка сохраняется между публикациями,
  а отмены, сделанные после получения списка, проверяются перед вызовом.
- **Исключения**. Шина доставляет событие всем доступным подписчикам и пробрасывает
  первое исключение после обхода.

### 3.2. Токен подписки (Subscription)
Класс `Subscription` инкапсулирует время жизни подписки:
```cpp
class Subscription {
public:
    Subscription() noexcept = default;
    ~Subscription() noexcept;

    Subscription(const Subscription &) = delete;
    Subscription &operator=(const Subscription &) = delete;
    Subscription(Subscription &&other) noexcept;
    Subscription &operator=(Subscription &&other) noexcept;

    void cancel() noexcept;
    bool isSubscribed() const noexcept;
};
```
- Move-only семантика. При разрушении или вызове `cancel()` отзывает регистрацию в `EventBus`.
- Слабая ссылка позволяет безопасно уничтожить токен после завершения работы шины.

### 3.3. Реактор ввода-вывода (Reactor)
Класс `Reactor` инкапсулирует событийный цикл:
```cpp
enum class IoEvent : unsigned {
    None = 0,
    Read = 1,
    Write = 2,
    HangUp = 4,
    Error = 8
};

using IoHandler = std::function<void(int fd, IoEvent events)>;

class Reactor {
public:
    using ErrorHandler = std::function<void(std::exception_ptr)>;

    explicit Reactor(ErrorHandler on_error = {});
    ~Reactor() noexcept;

    void addHandler(int fd, IoEvent events, IoHandler handler);
    void modifyHandler(int fd, IoEvent events);
    void removeHandler(int fd) noexcept;

    TimerHandle addTimer(std::chrono::milliseconds interval, bool periodic,
                         std::function<void()> callback);

    void post(std::function<void()> task);
    template <typename Event>
    void post(EventBus &bus, Event &&event);

    void run();
    int step(int timeout_ms = -1);
    void stop() noexcept;

    bool isRunning() const noexcept;
};
```
- **Мультиплексирование I/O**. Реактор опрашивает снимок регистраций через `poll(2)`
  и вызывает коллбэки в порядке регистрации. Снятая регистрация пропускается даже при
  готовности в текущем опросе. Новая регистрация учитывается при следующем опросе.
- **Межпоточное пробуждение**. `post()` добавляет задачу в FIFO-очередь под мьютексом
  и записывает `uint64_t{1}` в Linux `eventfd` с `EFD_NONBLOCK | EFD_CLOEXEC`.
  При переполнении счётчика `eventfd` уже готов к чтению. При другой ошибке записи
  постановка отменяется. Типизированное событие хранится в задаче с возможностью
  перемещения. Если шина к моменту обработки уничтожена, доставка пропускается.
- **Таймеры**. Для ожидания берётся ближайший срок по `steady_clock` или меньший
  таймаут `step()`. При `EINTR` используется остаток времени. Таймер, готовый
  к началу диспетчеризации, вызывает обработчик максимум один раз за итерацию.
  Повторяющийся таймер отсчитывает новый интервал после завершения обработчика.
- **Диспетчеризация**. После I/O выполняются задачи и таймеры, готовые к началу
  обработки. Новая работа, добавленная коллбэком, доступна на следующей итерации.
  При `stop()` реактора необработанные элементы очереди остаются для следующего запуска.
  Исключение коллбэка передаётся `on_error`, если он задан.

## 4. Владение и потокобезопасность

- **Thread Confinement**: Цикл реактора (`run()`, `step()`) исполняется строго в одном потоке (главном потоке приложения). Все обработчики I/O, таймеров и задач из очереди `post()` вызываются последовательно на этом потоке. Это позволяет подписчикам бизнес-логики работать без внутренних мьютексов.
- **Межпоточный доступ**: Методы `post()` и `stop()` реактора, а также регистрация/отмена подписок `EventBus` полностью потокобезопасны.
- Дескрипторы, переданные в `addHandler`, заимствуются реактором без передачи владения. Вызывающий код отвечает за их время жизни.

## 5. Схема рефакторинга src/main.cpp

1. stdin регистрируется в `Reactor`. Адаптер `src/ConsoleInput.h` ограничивает размер
   строки и объём чтения за итерацию. Готовая строка публикуется как `ConsoleCommandEvent`.
2. Обработчики `SIGINT` и `SIGTERM` выставляют lock-free флаг и неблокирующей записью
   будят существующий self-pipe. Его коллбэк в потоке реактора публикует
   `StopRequestedEvent`. EOF и команда `exit` используют тот же тип события.
3. Коллбэк `SaveWorker::eventFd()` собирает результаты и публикует `persistence::SaveResult`.
4. После `reactor.run()` приложение снимает обработчики stdin и сигналов. Ожидание
   свободного слота для финального снимка обслуживает `step()` с каналом сохранений.
   Сбой коллбэка runtime останавливает этот цикл и передаётся штатной обработке ошибок.

## 6. Стратегия тестирования

- Модульные тесты `tests/EventsTests.cpp`:
  - Изоляция типов: подписчик на `EventA` не получает `EventB`.
  - Автоматическая отписка: разрушение `Subscription` прекращает доставку.
  - Реентерабельность: публикация события из обработчика другого события.
  - Межпоточный `post()`: отправка задач из нескольких рабочих потоков `std::thread`, проверка выполнения на потоке реактора.
  - Точность таймеров: проверка срабатывания разовых и периодических таймеров, отмена через `TimerHandle`.
  - Опрос дескрипторов: проверка готовности неблокирующих каналов (`pipe`).
  - Остановка: корректный выход из `reactor.run()` по `reactor.stop()`.
- Регрессионные тесты CTest `runtime_*_tests` и `logging_cli_tests`.
