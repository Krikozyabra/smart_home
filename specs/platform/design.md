# Технические решения модуля platform

Статус: проектирование; основание — [spec.md](spec.md), `AGENTS.md`, реализация в `src/main.cpp`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), C++17.

## 1. Цели и границы модуля

Модуль инкапсулирует системные вызовы Linux POSIX, предоставляя безопасные C++17 примитивы:
- RAII-управление дескрипторами и временными каталогами;
- Неблокирующий канал уведомлений и self-pipe trick для сигналов ОС;
- RAII-модификацию флагов ввода-вывода дескрипторов;
- Мультиплексирование дескрипторов событий через `poll(2)`;
- Системную обработку ошибок (`errno`).

Бизнес-логика runtime, парсинг консольных команд и управление устройствами остаются в `src/main.cpp` и соответствующих модулях. Платформонезависимая абстракция под Windows/macOS не входит в область задач.

## 2. Архитектура и зависимости

- Пространство имён: `smart_home::platform`.
- Публичный интерфейс C++17 в `platform/include/platform/`.
- Статическая библиотека `smart_home_platform` с alias `SmartHome::Platform`.
- Зависимости CMake: `SmartHome::Platform` не имеет внешних SDK-зависимостей, кроме системных заголовков Linux/POSIX (`<fcntl.h>`, `<poll.h>`, `<signal.h>`, `<unistd.h>`).
- Приложение `smart_home` линкует `SmartHome::Platform` как `PRIVATE`.

## 3. Проектирование примитивов

### 3.1. FileDescriptor
Класс `FileDescriptor` обеспечивает эксклюзивное RAII-владение дескриптором `int`.
- Move-only (копирование запрещено, перемещение передаёт владение с обнулением источника в `-1`).
- `get()` — получение сырого дескриптора без передачи владения.
- `release()` — передача владения вызывающему коду (возвращает `fd`, внутреннее значение сбрасывается в `-1`).
- `reset(int fd = -1)` — закрытие текущего дескриптора (если валиден) и захват нового.
- `close()` — безопасное закрытие дескриптора. В соответствии с семантикой Linux дескриптор закрывается сразу, `EINTR` на Linux повторно `close()` не вызывает.
- `valid()`, `operator bool()` — проверка валидности (`fd >= 0`).

### 3.2. EventPipe
Класс `EventPipe` реализует неблокирующий канал оповещения.
- В конструкторе вызывает `::pipe2(fds, O_NONBLOCK | O_CLOEXEC)`. При ошибке выбрасывает `std::runtime_error`.
- Владеет читающим и пишущим `FileDescriptor`.
- `notify()`: потокобезопасная и async-signal-safe запись 1 байта (`::write(writer, &byte, 1)`). При возврате `EAGAIN`/`EWOULDBLOCK` ошибка игнорируется, так как в канале уже есть непрочитанный байт. При других кодах ошибок выбрасывается исключение (если не в контексте обработчика сигналов).
- `drain()`: полное вычитывание буфера до `EAGAIN` (`::read(reader, buf, sizeof(buf))`).
- `readerFd()`: доступ к дескриптору для регистрации в `Poller`.

### 3.3. SignalHandler
Класс `SignalHandler` обеспечивает перехват `SIGINT` и `SIGTERM` через POSIX self-pipe trick.
- Внутреннее статическое состояние:
  - `std::atomic<bool> g_stop_requested{false}`;
  - `std::atomic<int> g_signal_writer{-1}`;
  Оба атомика снабжены `static_assert(std::atomic<...>::is_always_lock_free)`.
- C-обработчик сигнала `extern "C" void signalDispatcher(int signum)`:
  - Сохраняет `errno`;
  - Атомарно выставляет `g_stop_requested = true`;
  - Если `g_signal_writer >= 0`, выполняет неблокирующий `::write(writer, &byte, 1)`;
  - Восстанавливает `errno`.
  Никаких блокировок, аллокаций и логов в обработчике сигнала.
- Конструктор `SignalHandler(const EventPipe &pipe)` регистрирует структуры `sigaction` с пустым `sa_mask` для `SIGINT` и `SIGTERM`, сохраняя старые диспозиции.
- Деструктор гарантированно восстанавливает исходные `sigaction` и сбрасывает `g_signal_writer` в `-1`.
- Методы: `isStopRequested()`, `requestStop()`, `eventFd()`, `drain()`.

### 3.4. NonBlockingScope
Класс `NonBlockingScope` переводит дескриптор в неблокирующий режим на время жизни объекта.
- Конструктор считывает текущие флаги через `fcntl(fd, F_GETFL)`, проверяет на ошибку и выставляет `fcntl(fd, F_SETFL, old | O_NONBLOCK)`.
- Деструктор возвращает исходные флаги через `fcntl(fd, F_SETFL, old_flags)`.

### 3.5. TemporaryDirectory
Класс `TemporaryDirectory` инкапсулирует безопасную работу с временными каталогами в RAM/FS.
- Конструктор принимает `base_path` и `prefix`, формирует шаблон `base / (prefix + "XXXXXX")` и вызывает `mkdtemp`.
- Деструктор проверяет флаг `preserve_`: если `preserve_ == false`, рекурсивно удаляет каталог через `std::filesystem::remove_all`.
- Метод `setPreserve(bool preserve)` позволяет зафиксировать каталог на диске при успешном завершении работы.

### 3.6. Poller
Класс `Poller` абстрагирует системный вызов `poll(2)`.
- `add(int fd, PollEvent events)` регистрирует дескриптор.
- `poll(int timeout_ms = -1)` выполняет `::poll()` в цикле `retryInterrupted`, возвращая число готовых дескрипторов.
- Методы проверки: `isReadable(fd)`, `isHangUp(fd)`, `hasError(fd)`, `isInvalid(fd)`.

### 3.7. SystemError & retryInterrupted
- `throwSystemError(operation)` формирует `std::runtime_error(operation + ": " + strerror(errno))`.
- `retryInterrupted(func)` повторяет выполнение системного вызова, пока он возвращает `-1` с `errno == EINTR`.

## 4. Владение и потокобезопасность

- Дескрипторы принадлежат строго объектам-обёрткам; неявного разделяемого владения дескрипторами нет.
- `EventPipe::notify()` полностью потокобезопасен и async-signal-safe.
- `SignalHandler` регистрируется только в основном потоке до старта рабочих потоков.
- `Poller` не аллоцирует память в цикле опроса дескрипторов.

## 5. Интеграция с src/main.cpp

- Из `src/main.cpp` полностью исключаются `<fcntl.h>`, `<poll.h>`, `<signal.h>`, `<unistd.h>`.
- Локальные типы заменяются на:
  - `Fd` -> `platform::FileDescriptor`;
  - `SignalPipe` -> `platform::EventPipe`;
  - `Signals` -> `platform::SignalHandler`;
  - `StdinFlags` -> `platform::NonBlockingScope`;
  - `Directory` -> `platform::TemporaryDirectory`;
  - `pollfd` + `::poll()` -> `platform::Poller`;
  - `systemError` -> `platform::throwSystemError`.

## 6. Стратегия тестирования

- Модульные тесты `tests/PlatformTests.cpp`:
  - `FileDescriptor`: move-семантика, `release`, `reset`, закрытие, двойной `close`;
  - `EventPipe`: `notify`, `drain`, пробуждение в `poll`, неблокирующее поведение;
  - `SignalHandler`: доставка `raise(SIGINT)`, установка флага, пробуждение пайпа, восстановление `sigaction`;
  - `NonBlockingScope`: проверка флагов дескриптора до, во время и после скоупа;
  - `TemporaryDirectory`: создание уникального пути, удаление при `preserve=false`, сохранение при `preserve=true`;
  - `Poller`: мультиплексирование нескольких пайпов, обработка `EINTR`, таймауты;
  - `SystemError`: корректность текста исключения, повторы в `retryInterrupted`.
- Регрессионные интеграционные тесты:
  - `python3 tests/RuntimeTests.py`;
  - `ctest -R LoggingCliTests`.
