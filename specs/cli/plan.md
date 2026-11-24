# План реализации модуля CLI

Основание — [spec.md](spec.md), [design.md](design.md), `AGENTS.md`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), CMake 3.20+, C++17.

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **CLI-001** | Интерфейс команды `ICliCommand` | Абстрактный класс с `name()`, `validate()`, `execute()` | Unit-тесты кастомной команды |
| **CLI-002** | Универсальный `ActionCommand` | Класс с проверкой числа аргументов и `std::function` | Тесты команд `scan`, `save`, `help`, `list`, `exit` |
| **CLI-003** | Специализированные команды | `ShowCommand`, `DeviceCommand`, `GroupCommand` | Тесты валидации ID, операций и параметров |
| **CLI-004** | Процессор `CliProcessor` | Реестр `unordered_map`, поиск $O(1)$, однократный токенайзер | Тесты быстрого поиска и сценариев исполнения |
| **CLI-005** | Команда впереди | Все команды первого уровня (`device`, `group`, ...) | Тесты синтаксиса `device 1 ...`, `group living_room ...` |
| **CLI-006** | Изоляция runtime | Обработчики `scan` и `save` инжектируются снаружи | Unit-тесты с вызовом мок-лямбд |
| **CLI-007** | Строгая валидация аргументов | Проверка типов, `0..100` для яркости, `0..255` для RGB | Тесты краевых и ошибочных значений аргументов |
| **CLI-008** | Категории `light`/`temperature_sensor` | Динамическое определение типа устройства по способностям | Тесты маппинга дескрипторов устройств |
| **CLI-009** | Абстракция вывода | `ICliOutputSink`, `StreamCliSink`, `StringCliSink` | Проверка накопления текста в `StringCliSink` |
| **CLI-010** | Безопасность ошибок | Возврат `CliStatus` без исключений наружу | Тесты неизвестных команд и неверных форматов |

## 2. Пошаговая структура работ (Work Breakdown Structure)

### Шаг 1: Создание спецификации и регистрация в проекте
- [x] 1.1. Создать каталог `specs/cli/`.
- [x] 1.2. Записать `specs/cli/spec.md`, `specs/cli/design.md` и `specs/cli/plan.md`.
- [x] 1.3. Зарегистрировать модуль в таблице `specs/README.md`.

### Шаг 2: Создание модуля `cli` и CMake
- [ ] 2.1. Создать каталог `cli/` со структурой `include/cli/` и `src/`.
- [ ] 2.2. Настроить `cli/CMakeLists.txt` с библиотекой `SmartHome::Cli` (C++17, `-Wall -Wextra -Wpedantic`).
- [ ] 2.3. Подключить `cli` в корневом `CMakeLists.txt` и `cmake/Docs.cmake`.

### Шаг 3: Реализация компонентов CLI
- [ ] 3.1. Реализовать `ICliOutputSink.h`, `StreamCliSink.h` и `StringCliSink.h`.
- [ ] 3.2. Реализовать `ICliCommand.h` и `ActionCommand.h`.
- [ ] 3.3. Реализовать `ShowCommand`, `DeviceCommand` и `GroupCommand`.
- [ ] 3.4. Реализовать `CliProcessor.h` и `CliProcessor.cpp` с токенизацией и поиском по `std::unordered_map`.

### Шаг 4: Модульные тесты CTest
- [ ] 4.1. Создать файл `tests/CliTests.cpp` с покрытием требований CLI-001–CLI-010.
- [ ] 4.2. Зарегистрировать цель `cli_tests` в `tests/CMakeLists.txt`.

### Шаг 5: Интеграция в `src/main.cpp` и обновление `RuntimeTests.py`
- [ ] 5.1. Подключить `SmartHome::Cli` в `src/CMakeLists.txt`.
- [ ] 5.2. Заменить внутренний ручной парсинг строк в `src/main.cpp` на `CliProcessor`.
- [ ] 5.3. Обновить строки команд устройств в `tests/RuntimeTests.py` (`device 1 ...`, `group living_room ...`).

### Шаг 6: Верификация
- [ ] 6.1. Сборка проекта и прохождение unit-тестов `cli_tests`.
- [ ] 6.2. Прохождение системных тестов `runtime_*_tests`.
- [ ] 6.3. Сборка документации Doxygen (`cmake --build build --target docs`).
