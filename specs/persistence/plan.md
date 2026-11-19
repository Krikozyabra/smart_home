# План реализации модуля persistence

Статус: модуль `SmartHome::Persistence` реализован и подключён к runtime.
Основание: [spec.md](spec.md), [design.md](design.md), `AGENTS.md`, реализация в `src/main.cpp`, тесты `tests/RuntimeTests.py`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), сборка CMake 3.20+, C++17.

---

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **PS-001** | Полное состояние сохраняется в виде изолированных нумерованных поколений (`state.sqlite`, `ontology.lbdb`, `metadata`) в `<state_dir>/generations/<gen_name>` | Классы `GenerationManager` и `SaveWorker`, формирование комплекта файлов | Модульные тесты `GenerationManagerTests`, `RuntimeTests.py` (`Persistence.test_crash_at_each_commit_boundary`) |
| **PS-002** | Манифест `metadata` с сигнатурой `SMART_HOME_RUNTIME 1`, ревизией и SHA-256 суммами обоих файлов | Функции вычисления SHA-256 через OpenSSL EVP API и парсинга манифеста в `GenerationManager` | Юнит-тесты `GenerationManagerTests`, тест повреждений `Persistence.test_corruption_never_resets` |
| **PS-003** | Таблица `device_properties` в `state.sqlite` с типизацией и транзакционное обновление ревизии в `runtime_meta` | `DeviceStateStore`: методы `storeProperties`, `loadProperties`, транзакции `BEGIN IMMEDIATE` / `COMMIT` | Модульные тесты `DeviceStateStoreTests` |
| **PS-004** | 8-шаговый протокол публикации поколения воркером `SaveWorker` | Метод `SaveWorker::publish`: `.pending-XXXXXX` -> fsync -> rename в `gen-*` -> fsync родителя -> `CURRENT.tmp` -> fsync -> rename `CURRENT` -> fsync `<state_dir>` -> `EventPipe` | Интеграционные тесты `RuntimeTests.py` (`Persistence`, `Failures`) |
| **PS-005** | Точка коммита — атомарная замена `CURRENT`; устойчивость к сбоям до и после коммита | Порядок операций в `SaveWorker::publish` и трактовка сбоев на шаге 7 как требующих повтора | Тест отказов на границах коммита `Persistence.test_crash_at_each_commit_boundary` |
| **PS-006** | `SaveWorker` оперирует исключительно RAM-снимками без доступа к живым объектам `Device` и SQLite/LadybugDB | `Snapshot` передаёт владение каталогом снимка в воркер | Тесты владения, очереди и событий. ThreadSanitizer проверяется отдельно |
| **PS-007** | Ограниченная очередь `capacity` и RAII-токен `Reservation` для backpressure | `SaveWorker::reserve()`, класс `Reservation`, отказ до мутации устройств | Тест переполнения очереди `Events.test_partial_input_worker_events_and_backpressure` |
| **PS-008** | Загрузка активного поколения при старте; чистый старт при отсутствии `CURRENT`; фатальная ошибка при повреждении | `GenerationManager::resolveCommitted()`, строгая валидация SHA-256 | Тесты `Persistence.test_corruption_never_resets` (7 сценариев повреждений) |
| **PS-009** | Сохранение состояния в RAM при ошибках диска и отложенный повтор коммита | Обработка исключений в `SaveWorker`, возврат `SaveResult{rev, false}`, флаг `unsaved` | Тест `Failures.test_retry_preparation_and_worker_failures` |
| **PS-010** | Ротация старых поколений после успешного коммита; ошибки ротации не отменяют коммит | Метод `SaveWorker::rotateGenerations` с перехватом исключений | Проверка каталога `generations/` в тестах `Persistence` |
| **PS-011** | Межпроцессная блокировка состояния через `flock` на `runtime.lock` | Класс `StateLock` с `LOCK_EX | LOCK_NB` | Тест параллельного запуска `Events.test_single_writer` |
| **PS-012** | Приватные тестовые точки (`Testing.h`) при `SMART_HOME_RUNTIME_TESTING=1`; no-op в релизе | Макрос `PERSISTENCE_TEST_POINT` и функция `testPoint` с контролем через FIFO и файлы | Тесты отказов в `RuntimeTests.py` (`Persistence`, `Failures`) |

---

## 2. Пошаговая структура работ (Work Breakdown Structure)

### Шаг 1: Создание структуры модуля `persistence/` и сборка CMake
- [x] 1.1. Создать каталог модуля `persistence/` со структурой:
  - `persistence/CMakeLists.txt`
  - `persistence/include/persistence/`
  - `persistence/src/`
- [x] 1.2. Описать таргет статической библиотеки `SmartHome::Persistence`:
  - Стандарт публичного интерфейса: C++17 (`target_compile_features(persistence PUBLIC cxx_std_17)`).
  - `SmartHome::Common` и `SmartHome::Devices` предоставляют публичные типы. SQLite3, OpenSSL Crypto, Threads и Logging подключаются приватно.
  - CTest использует сборку с `SMART_HOME_RUNTIME_TESTING=1`. Для `smart_home` тестовые точки включаются одноимённой опцией CMake.
- [x] 1.3. Определить базовые типы в `persistence/include/persistence/PersistenceTypes.h`:
  - `using Revision = std::int64_t;`
  - Структура `Snapshot` (владелец пути RAM-директории и номер `Revision`).
  - Структура `SaveResult` (`Revision revision`, `bool success`).
  - Структура `PropertyValue` (типизированный вариант и строковое представление).
  - Структура `CommittedGeneration` (`fs::path path`, `Revision revision`, `std::string name`).
- [x] 1.4. Реализовать RAII-класс `StateLock` (`include/persistence/StateLock.h`, `src/StateLock.cpp`):
  - Открытие дескриптора `<state_dir>/runtime.lock` с флагами `O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW`.
  - Захват неблокирующего `flock(fd, LOCK_EX | LOCK_NB)`.
  - Выброс `std::runtime_error("Runtime state is locked by another process.")` при занятости.
  - Закрытие дескриптора в деструкторе.

### Шаг 2: Реализация `DeviceStateStore` и модульных тестов схемы SQLite
- [x] 2.1. Создать приватные файловые утилиты `persistence/src/FileUtils.h` и `FileUtils.cpp`:
  - `syncFile()` и `syncDirectory()` с обработкой `EINTR`.
  - `createStateDirectories(const fs::path &path)` с синхронизацией созданных каталогов.
  - Безопасное чтение и запись текстовых файлов.
- [x] 2.2. Реализовать класс `DeviceStateStore` (`include/persistence/DeviceStateStore.h`, `src/DeviceStateStore.cpp`):
  - Управление жизненным циклом `sqlite3*` через RAII (`std::unique_ptr` с кастомным делетером `sqlite3_close_v2`).
  - Метод `initializeSchema()`: создание таблиц `runtime_meta(version, revision)` и `device_properties(device_id, property_key, value_type, raw_value, updated_at)`.
  - Метод `loadProperties(Revision expected_rev)`:
    - Проверка целостности `PRAGMA integrity_check`.
    - Проверка ревизии и версии в `runtime_meta`.
    - Чтение параметров устройств и десериализация типов (`bool`, `int`, `double`, `color`, `string`).
    - Восстановление прежних поколений с `runtime_values` и переход на `device_properties` при следующей записи.
  - Метод `storeProperties(const DevicePropertiesMap &properties, Revision revision)`:
    - Выполнение в транзакции `BEGIN IMMEDIATE` ... `COMMIT` (с `ROLLBACK` в блоке `catch`).
    - Замена полного набора свойств через подготовленный `sqlite3_stmt`.
    - Обновление ревизии в `runtime_meta`.
  - Метод `backupTo(const fs::path &target_path)`:
    - Инициализация и пошаговое выполнение `sqlite3_backup` в целевой файл на tmpfs/ramfs.
    - Проверка кодов возврата `SQLITE_DONE` и `SQLITE_OK`.
- [x] 2.3. Создать модульные тесты `tests/PersistenceStateStoreTests.cpp`:
  - Проверка создания чистой схемы.
  - Проверка транзакционного сохранения и повторной загрузки свойств различных типов.
  - Проверка отката транзакции при искусственной ошибке.
  - Проверка SQLite Backup API и целостности резервной копии.

### Шаг 3: Реализация `GenerationManager` и манифестов SHA-256
- [x] 3.1. Реализовать класс `GenerationManager` (`include/persistence/GenerationManager.h`, `src/GenerationManager.cpp`):
  - Приватная функция `fileSha256()`:
    - Использование OpenSSL EVP API (`EVP_MD_CTX_new`, `EVP_DigestInit_ex(EVP_sha256())`, `EVP_DigestUpdate`, `EVP_DigestFinal_ex`).
    - Чтение файла блоками по 64 КБ с проверкой ошибок чтения и EOF.
    - Формирование hex-строки из 64 символов.
  - Метод `formatManifest(Revision revision, const std::string &sqlite_hash, const std::string &lbdb_hash)`:
    - Формирование строки вида `SMART_HOME_RUNTIME 1\n<rev>\n<hash1>\n<hash2>\n`.
  - Метод `resolveCommitted()`:
    - Проверка наличия указателя `CURRENT`. Если отсутствует — возвращает `std::nullopt` (первый запуск).
    - Чтение содержимого `CURRENT`: проверка длины, символов имени поколения (`gen-<rev>-<suffix>`), запрет выхода из каталога (`../`).
    - Проверка существования каталога `<state_dir>/generations/<gen_name>`.
    - Чтение манифеста `metadata`, парсинг заголовка `SMART_HOME_RUNTIME 1` и номера ревизии.
    - Пересчёт SHA-256 файлов `state.sqlite` и `ontology.lbdb` и сверка с манифестом.
    - Сверка номера ревизии в имени, манифесте и `runtime_meta`.
    - Выброс `std::runtime_error("Corrupt or mixed runtime generation")` при любом несовпадении.
- [x] 3.2. Создать модульные тесты `tests/PersistenceGenerationManagerTests.cpp`:
  - Проверка сценария первого запуска при отсутствии `CURRENT`.
  - Проверка успешной валидации корректного поколения.
  - Проверка отбраковки некорректных имен `CURRENT` (выход за пределы каталога, мусорные символы, отсутствие перевода строки).
  - Проверка выброса фатальной ошибки при несовпадении ревизии, подмене файла `state.sqlite`, повреждении файла онтологии или порче манифеста.

### Шаг 4: Реализация `SaveWorker`, `Reservation` и протокола публикации
- [x] 4.1. Реализовать приватный заголовок `persistence/src/Testing.h`:
  - Функция `testPoint(const char *point, Revision revision)`.
  - Макрос `PERSISTENCE_TEST_POINT(point, revision)`.
  - Чтение переменной `SMART_HOME_RUNTIME_TEST_CONTROL`, создание `.reached`, ожидание на FIFO `.gate`, выброс ошибки при наличии `.fail`.
- [x] 4.2. Реализовать класс `SaveWorker` (`include/persistence/SaveWorker.h`, `src/SaveWorker.cpp`):
  - Потокобезопасная ограниченная очередь `std::deque<Snapshot>` с ограничением `capacity_`.
  - Публичный `reserve()` возвращает токен. Приватный `release()` возвращает неиспользованный слот.
  - Класс-обертка `Reservation` (`include/persistence/Reservation.h`, `src/Reservation.cpp`):
    - RAII-управление вызовом `release()` при разрушении неиспользованного токена.
    - Метод `submit(Snapshot)` для передачи снимка в воркер со сбросом владения.
  - Канал событий `EventPipe` для неблокирующего уведомления основного цикла (дескриптор для `poll`).
  - Публикация снимка с вызовами `PERSISTENCE_TEST_POINT`:
    1. `testPoint("before-files", rev)`
    2. Копирование `state.sqlite` и `ontology.lbdb` с вычислением SHA-256, запись `metadata` и `fsync` всех файлов
    3. `fsync` каталога `.pending-XXXXXX`
    4. `rename` каталога в `gen-<rev>-<id>`, `fsync(generations/)`, `testPoint("generation", rev)`
    5. Запись `CURRENT.tmp` и `fsync`, `testPoint("before-current", rev)`
    6. `rename(CURRENT.tmp, CURRENT)`, `testPoint("after-current", rev)`
    7. `fsync(state_dir)`, `testPoint("committed", rev)`
    8. Ротация старых поколений внутри `generations/`. Её ошибка журналируется, результат фиксации остаётся успешным.
  - Метод `workLoop()`:
    - Передача результата `success = false` реактору при ошибке фиксации. Реактор сообщает об отказе и сохраняет состояние для повторной попытки.
    - Оповещение через `event_pipe_.notify()`.
  - Метод `collectResults()`:
    - Считывание и очистка буфера `results_`, сброс байтов из `event_pipe_`, освобождение занятых слотов.
  - Метод `stop()`: корректная остановка потока и `join()`.
- [x] 4.3. Создать модульные тесты `tests/PersistenceWorkerTests.cpp`:
  - Проверка заполнения очереди и отказа `reserve()` при исчерпании `capacity`.
  - Проверка автоматического возврата слота в деструкторе `Reservation` при исключении.
  - Проверка асинхронной публикации и получения результата через дескриптор `EventPipe`.
  - Проверка ротации устаревших каталогов поколений.

### Шаг 5: Интеграция с приложением и сквозная верификация тестами
- [x] 5.1. Подключить `SmartHome::Persistence` в `src/CMakeLists.txt` и `tests/CMakeLists.txt`.
- [x] 5.2. Адаптировать `src/main.cpp`:
  - Заменить локальные классы `Database`, `SaveWorker`, `Reservation` на вызовы компонентов `smart_home::persistence`.
  - Использовать `DeviceStateStore` для сохранения значений устройств и `backupTo`.
  - Использовать `GenerationManager` для валидации поколения при старте.
- [x] 5.3. Запустить сквозные тесты `tests/RuntimeTests.py`:
  - `runtime_persistence_tests` (проверка сбоев на каждой из границ коммита и валидации повреждений).
  - `runtime_failures_tests` (проверка повторных попыток сохранения и сохранения состояния в RAM).
  - `runtime_events_tests` (проверка backpressure и сигналов).

---

## 3. План верификации

### 3.1. Юнит-тесты компонентов
```sh
# Сборка модульных тестов persistence
cmake -B build -S .
cmake --build build --target persistence_state_store_tests persistence_generation_manager_tests persistence_worker_tests -j4

# Запуск юнит-тестов
ctest --test-dir build -R persistence_ -V
```

### 3.2. Интеграционные тесты с симуляцией сбоев питания (`RuntimeTests.py`)
```sh
# Сборка тестового приложения с внедрением отказов
cmake --build build --target runtime_test_app -j4

# Запуск интеграционных наборов CTest
ctest --test-dir build -R "runtime_(persistence|failures|events)_tests" --output-on-failure
```

Критерии успеха проверки:
1. `Persistence.test_crash_at_each_commit_boundary`:
   - Аварийное завершение процесса по `SIGKILL` на любой точке до `after-current` гарантированно оставляет систему на предыдущей валидной ревизии.
   - После `SIGKILL` на точках `after-current` и `committed` видна новая ревизия. Точка `after-current` предшествует `fsync` каталога, поэтому при физическом сбое питания долговечность ещё неопределённа.
   - Каждая проверка запускает приложение повторно и проверяет согласованность и восстановление выбранного комплекта.
2. `Persistence.test_corruption_never_resets`:
   - Повреждение указателя, комплекта или значений завершает запуск с кодом 1 и сохраняет `CURRENT`. Несовпадение контрольных сумм или ревизии сообщает `"Corrupt or mixed runtime generation"`.
3. `Failures.test_retry_preparation_and_worker_failures`:
   - При отказе диска изменения остаются в оперативной памяти (RAM) и могут быть повторно сохранены командой `save`.
4. `Events.test_partial_input_worker_events_and_backpressure`:
   - При заполнении очереди фонового сохранения изменение отклоняется до выполнения, предотвращая переполнение памяти Raspberry Pi 4b.

## 4. Результаты проверок

| Команда или проверка | Результат |
|---|---|
| `cmake -B build -S .`, `cmake --build build -j4` | Сборка приложения, persistence и тестов успешна |
| `ctest --test-dir build --output-on-failure` | 32 из 32 наборов прошли в финальном прогоне |
| `ctest --test-dir build -R '^ontology_dump_tests$' --repeat until-fail:5 --output-on-failure` | Пять успешных повторов после одиночного сбоя сравнения снимков в параллельном прогоне |
| `cmake --build build --target docs` | Документация Doxygen с публичным API persistence собрана |
| `PersistenceWorkerTests.cpp` с `-fsanitize=thread` | Тест воркера прошёл с инструментированным модулем persistence |
| Release-сборка `smart_home` с `-DSMART_HOME_RUNTIME_TESTING=1` и `-DBUILD_TESTING=OFF` | Опция включает тестовые точки. В стандартной сборке CTest они доступны в `runtime_test_app` |
| `clang-format --dry-run --Werror`, `git diff --check` | Проверки форматирования прошли |

На Raspberry Pi и целевой SD-карте проверки не выполнялись. Сценарии
`SIGKILL` проверяют границы публикации процесса. Физическое отключение
питания и измерения на плате требуют отдельного прогона.
