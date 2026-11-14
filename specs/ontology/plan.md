# План развития модуля ontology (Capability-Based архитектура)

Статус: согласован со спецификациями [spec.md](spec.md), [design.md](design.md) и [specs/device_management/spec.md](../device_management/spec.md).  
Основание: `specs/ontology/spec.md`, `specs/ontology/design.md`, `specs/device_management/spec.md`, `AGENTS.md`.

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **ON-001** | CRUD операции узлов `rooms`, `devices`, `abilities` без статического поля `type` | Методы `add*`, `get*`, `getAll*`, `delete*` в `LadybugOntologyStorage` | Модульные тесты `OntologyStorageTests` |
| **ON-002** | Ограничение связей: 1 комната (`LOCATED_IN`), множество возможностей (`HAS_FEATURE`) | Транзакционные Cypher-запросы с удалением старой связи комнаты и идемпотентным `MERGE` для возможностей | Тесты связей в `OntologyStorageTests` |
| **ON-003** | Запросы связей и `getDeviceDetails` в порядке ключей | Запросы Cypher с `ORDER BY`, упаковка в `DeviceOntologyDetails` без `type` | Тесты выборки деталей и списков |
| **ON-004** | RAM-First рабочая копия в `/dev/shm` | Копирование базы при старте, работа только в RAM | Тесты создания и проверки прав каталога в `OntologyConfigTests` |
| **ON-005** | Атомарный протокол снимка (`CHECKPOINT -> copy -> fsync -> rename -> fsync`) | Метод `sync()`, отказоустойчивость до и после `rename` | Тесты снимков с внедрением сбоев в `OntologySyncTests` |
| **ON-006** | Сериализация операций и межпроцессная блокировка `flock` | `std::mutex` для потоков экземпляра, `.lock` файл с `flock` для процессов | Тесты конкурентности в `OntologyConcurrencyTests` |
| **ON-007** | Явный `close()`, остановка worker, безопасный деструктор | Остановка потока через `condition_variable`, финальный `sync()`, очистка RAM | Тесты жизненного цикла и аварийного выхода |

## 2. Этапы выполнения работ

- [x] 1. **Рефакторинг спецификаций модуля `ontology`**:
  - Обновить [specs/ontology/spec.md](spec.md) для полного согласования с Capability-Based архитектурой `device_management`.
  - Зафиксировать схему таблицы `devices(id INT64, name STRING, PRIMARY KEY(id))`.
  - Оформить технические решения в [specs/ontology/design.md](design.md).
  - Сформировать план реализации в [specs/ontology/plan.md](plan.md).
- [x] 2. **Обновление структур данных и интерфейса (C++)**:
  - В `ontology/include/ontology/OntologyRecords.h` исключить поле `std::string type` из структуры `OntologyDeviceRecord` и скорректировать операторы сравнения.
  - Актуализировать структуру `DeviceOntologyDetails`.
  - Обновить Doxygen-документацию в `ontology/include/ontology/IOntologyStorage.h`.
- [x] 3. **Корректировка реализации `LadybugOntologyStorage`**:
  - Обновить SQL/Cypher создание таблицы узлов: `CREATE NODE TABLE devices(id INT64, name STRING, PRIMARY KEY(id))`.
  - Обновить проверку схемы при открытии существующей базы данных.
  - Скорректировать запросы `addDevice`, `getDevice`, `getAllDevices`, `getDevicesInRoom`, `getDevicesWithAbility`, `getDeviceDetails` (удалить параметр/возврат `type`).
  - Убрать проверку `nonempty(device.type)` в валидаторе аргументов.
- [x] 4. **Адаптация тестов онтологии**:
  - Обновить вызовы `addDevice` в `tests/OntologyStorageTests.cpp`, `tests/OntologySyncTests.cpp`, `tests/OntologyConcurrencyTests.cpp`.
  - Проверить валидацию схемы в тестах.
- [ ] 5. **Интеграция с `device_management` и `runtime`**:
  - Отложена по указанию пользователя: текущая задача ограничена ontology,
    его тестами и документацией; runtime не изменяется.
  - Устранить вызовы фиктивного `type` в `device_management/src/DeviceManager.cpp` и `src/main.cpp`.
  - Реализовать фильтрацию устройств в `runtime` консоли через `getDevicesWithAbility`.
  - Текущий `DeviceManager` не обращается к ontology; включение графа в менеджер
    остаётся отдельной задачей по `specs/device_management/`.
  - Текущий `src/main.cpp` использует удалённое поле `type`; полная сборка
    приложения ожидает будущей адаптации runtime. Для проверки модуля собирать
    только цели `ontology_*_tests`.

## 3. План верификации

### Автоматизированные тесты
```sh
# Проверка изменений в спецификациях
git diff specs/ontology/

# Проверка сборки документации Doxygen
cmake -B build -S . -DBUILD_DOCS=ON
cmake --build build --target ontology_config_tests ontology_storage_tests ontology_sync_tests ontology_concurrency_tests -j2
cmake --build build --target docs

# Прогон тестов (после реализации C++ изменений)
ctest --test-dir build -R ontology --output-on-failure
```

## 4. Результаты реализации (2026-11-14)

Выполнены этапы 2–4. Запись устройства и все её проекции содержат только
`id` и `name`; выборки ролей используют связи с возможностями. Файловый
протокол снимков, блокировки и жизненный цикл сохранены.
Этап 5 отложен: файлы `src/`, `device_management/`, `specs/runtime/`
и runtime-тесты не изменены.

Проверки выполнены в отдельном каталоге `/tmp/smart-home-ontology/build`
с установленным Ladybug SDK 0.22.3, внутренним C++20 и публичным C++17.

| Команда | Результат |
|---|---|
| `cmake -B /tmp/smart-home-ontology/build -S . -DCMAKE_BUILD_TYPE=Debug -DBUILD_DOCS=ON -DSMART_HOME_LADYBUG_SDK_DIR=/home/prince/Projects/smart_home/libs` | Успех; SDK прошёл штатную проверку компиляции, линковки и запуска |
| `cmake --build /tmp/smart-home-ontology/build --target ontology_config_tests ontology_storage_tests ontology_sync_tests ontology_concurrency_tests -j2` | Все четыре тестовые цели собраны |
| `ctest --test-dir /tmp/smart-home-ontology/build -R '^ontology_' --output-on-failure -j2` | 4/4 набора прошли; 2,78 с, включая настоящий `/dev/shm` |
| `c++ -std=c++17 -Wall -Wextra -Wpedantic -fsyntax-only -Iontology/include -Icommon/include -x c++ -` | Успех для входа с публичным заголовком `LadybugOntologyStorage.h`, инициализацией и structured binding записи из двух полей, вызовами API |
| `cmake --build /tmp/smart-home-ontology/build --target docs` | Успех, Doxygen 1.18.0 |
| `git diff --check` | Успех |
| `git diff --exit-code -- src device_management specs/runtime tests/RuntimeTests.py` | Успех; отложенные компоненты не изменены |

Новые проверки покрывают проекции всех запросов устройств, независимость
возвращённых копий, сортировку, изменение набора возможностей, несколько
категорий возможностей у одного устройства, параметризацию имени,
сохранение связей при переименовании, перезапуск, пустое имя/NUL и отказ
открытия старой полной схемы с полем `type` без повреждения исходной базы.

Полная сборка приложения и runtime-тесты не запускались: текущий runtime
не совместим с удалённым полем и ожидает отдельного обновления.
ThreadSanitizer и проверки на Raspberry Pi в этой работе не выполнялись.
