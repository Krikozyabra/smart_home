# План развития модуля ontology (Унифицированный dump и удаление таймера)

Статус: согласован со спецификациями [spec.md](spec.md), [design.md](design.md) и [specs/storage/spec.md](../storage/spec.md).
Основание: `specs/ontology/spec.md`, `specs/ontology/design.md`, `specs/storage/spec.md`, `AGENTS.md`.

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **ON-001** | CRUD операции узлов `rooms`, `devices`, `abilities` без статического поля `type` | Методы `add*`, `get*`, `getAll*`, `delete*` в `LadybugOntologyStorage` | Модульные тесты `OntologyStorageTests` |
| **ON-002** | Ограничение связей: 1 комната (`LOCATED_IN`), множество возможностей (`HAS_FEATURE`) | Транзакционные Cypher-запросы с удалением старой связи комнаты и идемпотентным `MERGE` для возможностей | Тесты связей в `OntologyStorageTests` |
| **ON-003** | Запросы связей и `getDeviceDetails` в порядке ключей | Запросы Cypher с `ORDER BY`, упаковка в `DeviceOntologyDetails` без `type` | Тесты выборки деталей и списков |
| **ON-004** | RAM-First рабочая копия в `/dev/shm`, синхронное сохранение, отсутствие таймера | Копирование базы при старте, работа только в RAM, исключение фонового потока worker | Тесты в `OntologyConfigTests`, проверка отсутствия потоков |
| **ON-005** | Атомарный снимок `dumpToFile(path)` (`CHECKPOINT -> copy -> fsync -> rename -> fsync -> reopen`) | Метод `dumpToFile(path)`, валидация пути, отказоустойчивость до и после `rename` | Тесты снимков в `OntologyDumpTests` |
| **ON-006** | Сериализация операций и межпроцессная блокировка `flock` | `std::mutex transactionMutex` для потоков экземпляра, `.lock` файл с `flock` для процессов | Тесты конкурентности в `OntologyConcurrencyTests` |
| **ON-007** | Явный `close()`, опциональный автодамп `dump_path`, безопасный деструктор | Метод `close()`, автодамп при заданном пути, очистка RAM, защита `logic_error` после закрытия | Тесты жизненного цикла и аварийного выхода |

## 2. Этапы выполнения работ

- [x] 1. **Рефакторинг схемы и Capability-Based модель**:
  - Схема таблицы `devices(id INT64, name STRING, PRIMARY KEY(id))`.
  - Удаление поля `type` из структур и запросов.
- [x] 2. **Удаление периодического таймера и фонового потока**:
  - Удалить `sync_interval` и `parseSyncInterval` из `ontology/include/ontology/OntologyConfig.h` и `ontology/src/OntologyConfig.cpp`.
  - Удалить чтение `SMART_HOME_ONTOLOGY_SYNC_SECONDS` из `configFromEnvironment()`.
  - Удалить `worker`, `workerMutex`, `workerCondition`, `workerLoop()` из `LadybugOntologyStorage.cpp`.
- [x] 3. **Унификация методов и конфигурации с модулем Storage**:
  - В `OntologyConfig` переименовать `persistent_path` в `database_path`; добавить поле `std::string dump_path = {};` для финального автодампа.
  - В `IOntologyStorage.h` удалить `virtual void sync() = 0;` и добавить:
    ```cpp
    virtual void dumpToFile(const std::string &path) = 0;
    virtual void close() = 0;
    ```
  - В `LadybugOntologyStorage.h` объявить `dumpToFile(const std::string &path)` и `close()`.
  - В `LadybugOntologyStorage.cpp` реализовать:
    - Метод `dumpToFile(path)` с валидацией пути, сбросом checkpoint, закрытием движка, копированием, fsync, rename, fsync каталога и повторным открытием.
    - Метод `close()` с опциональным автодампом по `dump_path`, очисткой RAM, сбросом lock и переводом объекта в `closed_ = true`.
    - Проверку `closed_` во всех методах интерфейса с выбросом `std::logic_error`.
    - Деструктор с вызовом `close()` внутри `try-catch` и логированием на уровне `Critical`.
- [x] 4. **Адаптация и расширение тестов**:
  - В `tests/OntologyConfigTests.cpp`: исключить проверки `sync_interval`, добавить проверки `dump_path`.
  - Переименовать `tests/OntologySyncTests.cpp` в `tests/OntologyDumpTests.cpp`:
    - Заменить `sync()` на `dumpToFile(path)`.
    - Добавить тесты дампа в произвольный путь.
    - Добавить тесты валидации путей (пустой, NUL, симлинк, директория, рабочий файл).
    - Добавить тесты `close()` с автодампом и без, идемпотентность, запрет вызовов после закрытия.
  - В `tests/OntologyConcurrencyTests.cpp`:
    - Исключить тесты ожидания периодического таймера worker.
    - Добавить проверку конкурентных вызовов `dumpToFile(path)` параллельно с чтением и записью графа.
    - Проверить конкурентный `close()`.
  - В `tests/CMakeLists.txt`: переименовать цель `ontology_sync_tests` в `ontology_dump_tests`.
- [x] 5. **Верификация**:
  - `cmake --build build --target ontology_config_tests ontology_storage_tests ontology_dump_tests ontology_concurrency_tests`
  - `ctest --test-dir build -R ontology --output-on-failure`
  - `cmake --build build --target docs`

## 3. План верификации

### Автоматизированные тесты
```sh
# Сборка тестов онтологии
cmake -B build -S . -DBUILD_DOCS=ON
cmake --build build --target ontology_config_tests ontology_storage_tests ontology_dump_tests ontology_concurrency_tests -j2
cmake --build build --target docs

# Запуск тестов онтологии
ctest --test-dir build -R ontology --output-on-failure
```

## 4. Результаты проверки

- `cmake -B build -S . -DBUILD_DOCS=ON` — успешно; локальный LadybugDB SDK 0.22.3,
  приватный C++20, публичные потребители C++17.
- `cmake --build build -j2` — собраны все цели, включая приложение и runtime-тесты.
- `cmake --build build --target ontology_config_tests ontology_storage_tests ontology_dump_tests ontology_concurrency_tests -j2`
  — успешно.
- `ctest --test-dir build --output-on-failure` — все 29 зарегистрированных тестов
  прошли, включая четыре набора ontology и проверки поколений runtime.
- `cmake --build build --target docs` — успешно, без предупреждений Doxygen.
- Дополнительно проверены независимые назначения dump без новых изменений,
  относительные пути после смены cwd, сохранение исходной базы при отдельном
  dump_path, повтор close после ошибки назначения, все методы после закрытия,
  отказ сохранения до/после rename и восстановление рабочей копии.

Замеры CPU/RAM и проверка на Raspberry Pi в эту верификацию не входили.
