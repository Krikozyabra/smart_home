# План реализации модуля storage (Унифицированное завершение и dump)

Статус: согласован со спецификацией [spec.md](spec.md) и [design.md](design.md).
Основание: `specs/storage/spec.md`, `specs/storage/design.md`, `specs/ontology/spec.md`, `AGENTS.md`.

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **ST-001..ST-003** | Базовые операции инвентаря (`insert`, `findByPhysicalId`, `getAll`) | Реализации в `InMemoryDeviceStorage` и `SQLiteDeviceStorage` | `StorageTests.cpp`, `SQLStorageTests.cpp` |
| **ST-004..ST-005** | Атомарный `dumpToFile(path)` по 6-этапному протоколу | `storage_detail::dumpToFile` через SQLite backup API, fsync и rename | `SQLiteDumpTests.cpp` |
| **ST-006** | Деструктор как безопасный `noexcept` fallback для автодампа | `~SQLiteDeviceStorage` вызывает `close()` с перехватом исключений и логированием | `SQLiteDumpTests.cpp` (проверка лога и отсутствия исключений) |
| **ST-007** | `dumpToFile(path)` через `IDeviceStorage` (InMemory даёт `logic_error`) | Чисто виртуальный метод в интерфейсе | `SQLiteDumpTests.cpp` |
| **ST-008** | Явный метод `close()`, идемпотентность, проброс ошибок, защита `logic_error` | Метод `close()` в `IDeviceStorage`, `SQLiteDeviceStorage`, `InMemoryDeviceStorage` | `StorageTests.cpp`, `SQLStorageTests.cpp`, `SQLiteDumpTests.cpp` |

## 2. Последовательность работ

- [ ] 1. **Обновление спецификаций и технических решений**:
  - Зафиксировать требование ST-008 в [specs/storage/spec.md](spec.md).
  - Сформировать [specs/storage/design.md](design.md) и [specs/storage/plan.md](plan.md).
- [ ] 2. **Интерфейс `IDeviceStorage` (`storage/include/storage/interfaces/IDeviceStorage.h`)**:
  - Объявить чисто виртуальный метод:
    ```cpp
    virtual void close() = 0;
    ```
  - Документировать поведение, идемпотентность и исключения на русском языке (Doxygen).
- [ ] 3. **Реализация в `InMemoryDeviceStorage`**:
  - Добавить поле `bool closed_ = false;`.
  - Реализовать метод `close()`: устанавливает `closed_ = true`.
  - Добавить проверку `closed_` в методах `insert`, `findByPhysicalId`, `getAll` с выбросом `std::logic_error("Storage is closed.")`.
- [ ] 4. **Реализация в `SQLiteDeviceStorage`**:
  - Добавить поле `bool closed_ = false;`.
  - Реализовать метод `close()`:
    - Проверка `if (closed_) return;`.
    - Если `!dump_path.empty()`, выполнить `dumpToFile(dump_path)`.
    - Закрыть соединение `sqlite3_close_v2(database)`.
    - Сбросить указатель `database = nullptr; closed_ = true;`.
    - При сбое автодампа или закрытия выбросить `std::runtime_error`.
  - Обновить методы `insert`, `findByPhysicalId`, `getAll`, `dumpToFile`: выброс `std::logic_error("Storage is closed.")` при `closed_ == true`.
  - Обновить деструктор `~SQLiteDeviceStorage()`:
    - Если `!closed_`, вызвать `close()` в блоке `try-catch`.
    - Ошибки логировать через `logging::log(logging::Level::Error, ...)`.
- [ ] 5. **Тестирование**:
  - Добавить проверки `close()` в `tests/StorageTests.cpp` для `InMemoryDeviceStorage`.
  - Добавить проверки `close()` в `tests/SQLStorageTests.cpp` для `SQLiteDeviceStorage`.
  - Добавить проверки автодампа в `close()`, проброса ошибок, идемпотентности и запрета вызовов после закрытия в `tests/SQLiteDumpTests.cpp`.
- [ ] 6. **Верификация**:
  - Сборка: `cmake --build build`.
  - Запуск тестов: `ctest --test-dir build -R storage --output-on-failure`.
  - Запуск тестов: `ctest --test-dir build -R sqlite_dump --output-on-failure`.
  - Сборка документации: `cmake --build build --target docs`.

## 3. План верификации

### Автоматизированные тесты
```sh
# Сборка
cmake --build build

# Запуск тестов хранилища
ctest --test-dir build -R '^storage_' --output-on-failure
ctest --test-dir build -R '^sqlite_dump' --output-on-failure

# Полный прогон тестов
ctest --test-dir build --output-on-failure

# Документация Doxygen
cmake --build build --target docs
```
