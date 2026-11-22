# Технические решения модуля events

Статус: проектирование; основание — [spec.md](spec.md), `AGENTS.md`, существующая реализация в `src/main.cpp`.
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
- **Реентерабельность**: При вызове `publish()` список активных коллбэков копируется под блокировкой внутреннего мьютекса, а сам вызов обработчиков происходит вне мьютекса. Это предотвращает дедлоки и порчу итераторов при вызове `publish()`, `subscribe()` или `unsubscribe()` изнутри обработчика события.
- **Исключения**: Если обработчик события выбрасывает исключение, шина гарантирует целостность своих внутренних структур.

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
    Reactor();
    ~Reactor() noexcept;

    void addHandler(int fd, IoEvent events, IoHandler handler);
    void modifyHandler(int fd, IoEvent events);
    void removeHandler(int fd) noexcept;

    TimerHandle addTimer(std::chrono::milliseconds interval, bool periodic,
                         std::function<void()> callback);

    void post(std::function<void()> task);

    void run();
    int step(int timeout_ms = -1);
    void stop() noexcept;

    bool isRunning() const noexcept;
};
```
- **Мультиплексирование I/O**: Реактор хранит массив дескрипторов для `poll(2)`. При готовности дескриптора вызывается зарегистрированный `IoHandler`.
- **Межпоточное пробуждение (`post`)**: Для пробуждения из фоновых потоков используется Linux `eventfd` с флагами `EFD_NONBLOCK | EFD_CLOEXEC`. Метод `post()` добавляет функциональный объект в защищённую мьютексом очередь и записывает 8-байтовое значение `1` в `eventfd`. Реактор считывает `eventfd` и исполняет задачи из очереди строго в контексте потока реактора.
- **Таймеры**: Хранятся в структуре с абсолютным временем срабатывания по `std::chrono::steady_clock`. При вычислении таймаута для `poll(2)` реактор находит ближайший таймер и передаёт остаток времени в `poll()`, гарантируя своевременное срабатывание без запаздываний.

## 4. Владение и потокобезопасность

- **Thread Confinement**: Цикл реактора (`run()`, `step()`) исполняется строго в одном потоке (главном потоке приложения). Все обработчики I/O, таймеров и задач из очереди `post()` вызываются последовательно на этом потоке. Это позволяет подписчикам бизнес-логики работать без внутренних мьютексов.
- **Межпоточный доступ**: Методы `post()` и `stop()` реактора, а также регистрация/отмена подписок `EventBus` полностью потокобезопасны.
- Дескрипторы, переданные в `addHandler`, заимствуются реактором без передачи владения. Вызывающий код отвечает за их время жизни.

## 5. Схема рефакторинга src/main.cpp

В `src/main.cpp`:
1. Устраняется массив `pollfd` и прямой вызов `::poll()`.
2. Ввод со `stdin` регистрируется как дескриптор в `Reactor`. При готовности данные вычитываются и собираются в строки; готовая строка публикуется в `EventBus` как `ConsoleCommandEvent`.
3. Сигналы `SIGINT`/`SIGTERM` через обработчик вызывают `reactor.post([&] { bus.publish(StopRequestedEvent{}); reactor.stop(); })`.
4. Дескриптор `SaveWorker::eventFd()` регистрируется в `Reactor`; при готовности результаты воркера собираются и публикуются в `EventBus` как `SaveCompletedEvent`.
5. Главный цикл сводится к:
   ```cpp
   reactor.run();
   ```
   с последующей штатной фиксацией и остановкой компонентов.

## 6. Стратегия тестирования

- Модульные тесты `tests/EventsTests.cpp`:
  - Изоляция типов: подписчик на `EventA` не получает `EventB`.
  - Автоматическая отписка: разрушение `Subscription` прекращает доставку.
  - Реентерабельность: публикация события из обработчика другого события.
  - Межпоточный `post()`: отправка задач из нескольких рабочих потоков `std::thread`, проверка выполнения на потоке реактора.
  - Точность таймеров: проверка срабатывания разовых и периодических таймеров, отмена через `TimerHandle`.
  - Опрос дескрипторов: проверка готовности неблокирующих каналов (`pipe`).
  - Остановка: корректный выход из `reactor.run()` по `reactor.stop()`.
- Регрессионные тесты:
  - `RuntimeTests.py` и `LoggingCliTests.cmake`.
