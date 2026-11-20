# План реализации модуля platform

Основание — [spec.md](spec.md), [design.md](design.md), `AGENTS.md`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), CMake 3.20+, C++17.

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **PL-001** | RAII-дескриптор `FileDescriptor` | Класс `FileDescriptor` с move-семантикой и закрытием в деструкторе | Модульные тесты перемещения, сброса, закрытия, двойного освобождения |
| **PL-002** | Неблокирующий канал `EventPipe` | `EventPipe` на `pipe2(O_NONBLOCK | O_CLOEXEC)`, методы `notify()` и `drain()` | Тест отправки байта, пробуждения poll и вычитывания буфера |
| **PL-003** | Перехват сигналов `SignalHandler` | Self-pipe trick, `sigaction(SIGINT/SIGTERM)`, lock-free atomics | Тест вызова `raise(SIGINT)`, проверка флага и восстановления обработчиков |
| **PL-004** | RAII-флаги `NonBlockingScope` | `fcntl(F_GETFL/F_SETFL, O_NONBLOCK)` с восстановлением флагов | Тест проверки флагов дескриптора до, во время и после скоупа |
| **PL-005** | Временные каталоги `TemporaryDirectory` | `mkdtemp`, флаг сохранения, рекурсивное удаление | Тест очистки при деструкции и сохранения после `preserve()` |
| **PL-006** | Мультиплексор `Poller` | Обёртка над `poll(2)` с обработкой `EINTR` и масками событий | Тест опроса пайпа с таймаутом, детекция `POLLIN`, `POLLHUP` |
| **PL-007** | Системные ошибки `SystemError` | `throwSystemError`, шаблон `retryInterrupted` | Проверка генерации исключений с кодом `errno` и повтора при прерывании |

## 2. Пошаговая структура работ (Work Breakdown Structure)

### Шаг 1: Структура каталогов и CMake
- [x] 1.1. Создать каталог `platform/` со структурой `include/platform/` и `src/`.
- [x] 1.2. Создать `platform/CMakeLists.txt` с таргетом `SmartHome::Platform` (C++17, `-Wall -Wextra -Wpedantic`).
- [x] 1.3. Подключить `platform` в корневой `CMakeLists.txt` и `cmake/Docs.cmake`.

### Шаг 2: Реализация базовых примитивов
- [x] 2.1. Реализовать `SystemError.h` и `SystemError.cpp` (`throwSystemError`, `retryInterrupted`).
- [x] 2.2. Реализовать `FileDescriptor.h` и `FileDescriptor.cpp`.
- [x] 2.3. Реализовать `EventPipe.h` и `EventPipe.cpp`.

### Шаг 3: Реализация сигналов и режимов дескрипторов
- [x] 3.1. Реализовать `SignalHandler.h` и `SignalHandler.cpp`.
- [x] 3.2. Реализовать `NonBlockingScope` в `DescriptorFlags` и чтение в `DescriptorIO`.
- [x] 3.3. Реализовать `TemporaryDirectory.h` и `TemporaryDirectory.cpp`.

### Шаг 4: Реализация мультиплексора
- [x] 4.1. Реализовать `Poller.h` и `Poller.cpp`.

### Шаг 5: Модульные тесты
- [x] 5.1. Создать `tests/PlatformTests.cpp` для проверки требований PL-001–PL-007.
- [x] 5.2. Зарегистрировать `platform_tests` в `tests/CMakeLists.txt`.

### Шаг 6: Рефакторинг `src/main.cpp`
- [x] 6.1. Заменить локальные обёртки на примитивы `smart_home::platform`.
- [x] 6.2. Удалить системные инклуды `<fcntl.h>`, `<poll.h>`, `<signal.h>`, `<unistd.h>`.
- [x] 6.3. Добавить зависимость `SmartHome::Platform` в `src/CMakeLists.txt`.

### Шаг 7: Верификация
- [x] 7.1. Собрать проект и запустить `platform_tests`.
- [x] 7.2. Запустить регрессионные тесты `runtime_*_tests` и `logging_cli_tests`.
- [x] 7.3. Собрать документацию Doxygen.

## 3. Область изменений реализации

| Файлы | Назначение |
|---|---|
| `platform/include/platform/*` | Публичные C++17 интерфейсы примитивов платформы |
| `platform/src/*` | Реализация системных вызовов и RAII-обёрток |
| `platform/CMakeLists.txt` | Таргет статической библиотеки `SmartHome::Platform` |
| `CMakeLists.txt` | Подключение `add_subdirectory(platform)` |
| `cmake/Docs.cmake` | Подключение нового API к документации |
| `src/main.cpp` | Очистка рантайма от низкоуровневых вызовов, использование примитивов `platform` |
| `src/CMakeLists.txt` | Линковка `SmartHome::Platform` к `smart_home` |
| `tests/PlatformTests.cpp` | Модульные тесты требований PL-001–PL-007 |
| `tests/CMakeLists.txt` | Регистрация исполняемого файла тестов в CTest |
| `specs/platform/*` | Спецификация, дизайн и план работ |

## 4. Автоматические проверки

- Сборка:
  ```bash
  cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DSMART_HOME_LADYBUG_SDK_DIR="$PWD/libs"
  cmake --build build --parallel
  ```
- Запуск юнит-тестов платформы:
  ```bash
  ctest --test-dir build -R '^platform_tests$' --output-on-failure
  ```
- Регрессионные тесты runtime:
  ```bash
  ctest --test-dir build -R '^(logging_cli_tests|runtime_.*_tests)$' --output-on-failure
  ```
- Документация (при `BUILD_DOCS=ON`):
  ```bash
  cmake --build build --target docs
  ```
