# План реализации модуля platform

Статус: планирование; основание — [spec.md](spec.md), [design.md](design.md), `AGENTS.md`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), CMake 3.20+, C++17.

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **PL-001** | RAII-дескриптор `FileDescriptor` | Класс `FileDescriptor` с move-семантикой и закрытием в деструкторе | Модульные тесты перемещения, сброса, закрытия, двойного освобождения |
| **PL-002** | Неблокирующий канал `EventPipe` | `EventPipe` на `pipe2(O_NONBLOCK | O_CLOEXEC)`, методы `notify()` и `drain()` | Тест отправки байта, пробуждения poll и вычитывания буфера |
| **PL-003** | Перехват сигналов `SignalHandler` | Self-pipe trick, `sigaction(SIGINT/SIGTERM)`, lock-free atomics | Тест вызова `raise(SIGINT)`, проверка флага и восстановления обработчиков |
| **PL-004** | RAII-флаги `NonBlockingScope` | `fcntl(F_GETFL/F_SETFL, O_NONBLOCK)` с восстановлением флагов | Тест проверки флагов дескриптора до, во время и после скоупа |
| **PL-005** | Временные каталоги `TemporaryDirectory` | `mkdtemp`, флаг `preserve`, рекурсивное удаление | Тест очистки при деструкции и сохранения при `preserve(true)` |
| **PL-006** | Мультиплексор `Poller` | Обёртка над `poll(2)` с обработкой `EINTR` и масками событий | Тест опроса пайпа с таймаутом, детекция `POLLIN`, `POLLHUP` |
| **PL-007** | Системные ошибки `SystemError` | `throwSystemError`, шаблон `retryInterrupted` | Проверка генерации исключений с кодом `errno` и повтора при прерывании |

## 2. Пошаговая структура работ (Work Breakdown Structure)

### Шаг 1: Структура каталогов и CMake
- [ ] 1.1. Создать каталог `platform/` со структурой `include/platform/` и `src/`.
- [ ] 1.2. Создать `platform/CMakeLists.txt` с таргетом `SmartHome::Platform` (C++17, `-Wall -Wextra -Wpedantic`).
- [ ] 1.3. Подключить `platform` в корневой `CMakeLists.txt`.

### Шаг 2: Реализация базовых примитивов
- [ ] 2.1. Реализовать `SystemError.h` и `SystemError.cpp` (`throwSystemError`, `retryInterrupted`).
- [ ] 2.2. Реализовать `FileDescriptor.h` и `FileDescriptor.cpp`.
- [ ] 2.3. Реализовать `EventPipe.h` и `EventPipe.cpp`.

### Шаг 3: Реализация сигналов и режимов дескрипторов
- [ ] 3.1. Реализовать `SignalHandler.h` и `SignalHandler.cpp`.
- [ ] 3.2. Реализовать `DescriptorFlags.h` и `DescriptorFlags.cpp` (`NonBlockingScope`).
- [ ] 3.3. Реализовать `TemporaryDirectory.h` и `TemporaryDirectory.cpp`.

### Шаг 4: Реализация мультиплексора
- [ ] 4.1. Реализовать `Poller.h` и `Poller.cpp`.

### Шаг 5: Модульные тесты
- [ ] 5.1. Создать `tests/PlatformTests.cpp` с полным покрытием требований PL-001–PL-007.
- [ ] 5.2. Зарегистрировать `PlatformTests` в `tests/CMakeLists.txt`.

### Шаг 6: Рефакторинг `src/main.cpp`
- [ ] 6.1. Заменить локальные классы `Fd`, `SignalPipe`, `Signals`, `StdinFlags`, `Directory`, `systemError` на примитивы `smart_home::platform`.
- [ ] 6.2. Удалить системные инклуды `<fcntl.h>`, `<poll.h>`, `<signal.h>`, `<unistd.h>`.
- [ ] 6.3. Добавить зависимость `SmartHome::Platform` в `src/CMakeLists.txt`.

### Шаг 7: Верификация
- [ ] 7.1. Сборка проекта и прохождение юнит-тестов `PlatformTests`.
- [ ] 7.2. Регрессионный запуск `RuntimeTests.py` и `LoggingCliTests.cmake`.

## 3. Область изменений реализации

| Файлы | Назначение |
|---|---|
| `platform/include/platform/*` | Публичные C++17 интерфейсы примитивов платформы |
| `platform/src/*` | Реализация системных вызовов и RAII-обёрток |
| `platform/CMakeLists.txt` | Таргет статической библиотеки `SmartHome::Platform` |
| `CMakeLists.txt` | Подключение `add_subdirectory(platform)` |
| `src/main.cpp` | Очистка рантайма от низкоуровневых вызовов, использование примитивов `platform` |
| `src/CMakeLists.txt` | Линковка `SmartHome::Platform` к `smart_home` |
| `tests/PlatformTests.cpp` | Модульные тесты требований PL-001–PL-007 |
| `tests/CMakeLists.txt` | Регистрация исполняемого файла тестов в CTest |
| `specs/platform/*` | Спецификация, дизайн и план работ |

## 4. Автоматические проверки

- Сборка:
  ```bash
  cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
  cmake --build build -- -j$(nproc)
  ```
- Запуск юнит-тестов платформы:
  ```bash
  ctest --test-dir build -R PlatformTests --output-on-failure
  ```
- Регрессионные тесты runtime:
  ```bash
  ctest --test-dir build -R LoggingCliTests --output-on-failure
  python3 tests/RuntimeTests.py
  ```
