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
- [ ] 2. **Обновление структур данных и интерфейса (C++)**:
  - В `ontology/include/ontology/OntologyRecords.h` исключить поле `std::string type` из структуры `OntologyDeviceRecord` и скорректировать операторы сравнения.
  - Актуализировать структуру `DeviceOntologyDetails`.
  - Обновить Doxygen-документацию в `ontology/include/ontology/IOntologyStorage.h`.
- [ ] 3. **Корректировка реализации `LadybugOntologyStorage`**:
  - Обновить SQL/Cypher создание таблицы узлов: `CREATE NODE TABLE devices(id INT64, name STRING, PRIMARY KEY(id))`.
  - Обновить проверку схемы при открытии существующей базы данных.
  - Скорректировать запросы `addDevice`, `getDevice`, `getAllDevices`, `getDevicesInRoom`, `getDevicesWithAbility`, `getDeviceDetails` (удалить параметр/возврат `type`).
  - Убрать проверку `nonempty(device.type)` в валидаторе аргументов.
- [ ] 4. **Адаптация тестов онтологии**:
  - Обновить вызовы `addDevice` в `tests/OntologyStorageTests.cpp`, `tests/OntologySyncTests.cpp`, `tests/OntologyConcurrencyTests.cpp`.
  - Проверить валидацию схемы в тестах.
- [ ] 5. **Интеграция с `device_management` и `runtime`**:
  - Устранить вызовы фиктивного `type` в `device_management/src/DeviceManager.cpp` и `src/main.cpp`.
  - Реализовать фильтрацию устройств в `runtime` консоли через `getDevicesWithAbility`.

## 3. План верификации

### Автоматизированные тесты
```sh
# Проверка изменений в спецификациях
git diff specs/ontology/

# Проверка сборки документации Doxygen
cmake -B build -S . -DBUILD_DOCS=ON
cmake --build build --target docs

# Прогон тестов (после реализации C++ изменений)
ctest --test-dir build -R ontology --output-on-failure
```
