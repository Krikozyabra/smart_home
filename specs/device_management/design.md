# Технические решения модуля device_management

Статус: согласовано со спецификацией [spec.md](spec.md); подготовлено к реализации.  
Основание: `specs/device_management/spec.md`, `specs/README.md`, `AGENTS.md`, архитектура модулей `storage`, `registry`, `ontology` и `runtime`.  
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), приложение C++17.

## 1. Цели, границы и концепция Capability-Based

Модуль `device_management` координирует инвентарь (`IDeviceStorage`), реестр фабрик (`DriverFactoryRegistry`), реестр активных драйверов (`DeviceRegistry`) и семантический граф онтологии (`ontology::IOntologyStorage`).

Архитектура построена на **компонентной модели возможностей (Capability-Based)**:
- Устройства не привязываются к жестким статическим категориям (типам вроде `"light"` или `"sensor"`).
- Назначение и функциональность устройства полностью определяются набором его связей `HAS_FEATURE` с записями каталога возможностей (`on_off`, `brightness`, `color`, `temperature` и др.).
- Модулю `device_management` не требуется внешняя таблица сопоставления драйверов и типов (`driver_type_mapping`).

### Границы ответственности
- **Входит в зону ответственности**:
  - Регистрация обнаруженных устройств: создание записи в инвентаре, создание активного драйвера через фабрику, регистрация драйвера в `DeviceRegistry`, создание узла устройства в онтологии и привязка связей `HAS_FEATURE` по дескриптору драйвера.
  - Восстановление сохранённых устройств: чтение инвентаря, атомарная предварительная подготовка отсутствующих драйверов, регистрация их в реестре, актуализация узлов и связей возможностей в онтологии.
  - Проверка консистентности: строгий контроль соответствия имени устройства и состава связей возможностям активного драйвера; отсутствие молчаливых перезаписей.
  - Идемпотентность: безопасные повторные вызовы регистрации и восстановления без дублирования драйверов, узлов и ребер.

- **Не входит в зону ответственности**:
  - Создание и управление комнатами (`RoomRecord`, связи `LOCATED_IN`).
  - Предварительное наполнение каталога возможностей (`AbilityRecord`).
  - Сохранение и восстановление runtime-значений возможностей (яркость, температура) — выполняется в `runtime`.
  - Управление долговечностью графа (`sync()`, `close()`, публикация поколений).
  - Фоновые потоки, периодический опрос и таймеры.
  - MQTT-сообщения и события сети.

## 2. Архитектура интерфейса

### Контракт класса `DeviceManager`
Файл: `device_management/include/device_management/DeviceManager.h`

```cpp
namespace smart_home {

class DeviceManager {
  private:
    IDeviceStorage &storage;
    const DriverFactoryRegistry &factory_registry;
    DeviceRegistry &device_registry;
    ontology::IOntologyStorage &ontology_storage;

    DeviceRecord getOrCreate(const std::string &physical_id,
                             const std::string &driver_id,
                             const std::string &default_name);

    void syncDeviceOntology(const DeviceRecord &record,
                            const IDeviceDriver &driver);

  public:
    DeviceManager(IDeviceStorage &storage,
                  const DriverFactoryRegistry &factory_registry,
                  DeviceRegistry &device_registry,
                  ontology::IOntologyStorage &ontology_storage);

    std::size_t restoreStoredDevices();

    DeviceRecord registerDiscoveredDevice(const std::string &physical_id,
                                          const std::string &driver_id,
                                          const std::string &default_name);
};

} // namespace smart_home
```

### Зависимости и сборка CMake
- `device_management/CMakeLists.txt` экспортирует `SmartHome::DeviceManagement`.
- `SmartHome::Storage`, `SmartHome::Registry` и `SmartHome::Ontology` подключаются как `PUBLIC`.
- `SmartHome::Logging` подключается как `PRIVATE`.
- C++ SDK LadybugDB инкапсулирован внутри `SmartHome::Ontology` и не проникает в публичные заголовки потребителей.

## 3. Алгоритмы и порядок операций

### 3.1. Регистрация обнаруженного устройства (`registerDiscoveredDevice`)
1. **Проверка доступности фабрики**:
   Поиск `factory_registry.find(driver_id)`. Если фабрика не найдена, выбрасывается `UnsupportedDriverError` до любых обращений к хранилищу и онтологии.
2. **Получение или создание инвентарной записи**:
   Вызывается `getOrCreate(physical_id, driver_id, default_name)`. При повторной регистрации возвращается существующая запись, `default_name` не меняет имя.
3. **Получение или инстанцирование драйвера**:
   Проверяется наличие активного драйвера в `device_registry.find(record.local_id)`.
   - Если драйвер уже активен, он сохраняется без изменений (сохраняя текущие runtime-значения).
   - Если драйвер отсутствует, вызывается `factory->create(record)`. Если фабрика вернула `nullptr`, выбрасывается `std::logic_error`. Созданный экземпляр передаётся в `device_registry.add(...)`.
4. **Синхронизация с онтологией (`syncDeviceOntology`)**:
   - Получение дескриптора активного драйвера (`driver->descriptor()`).
   - Проверка наличия узла устройства: `ontology_storage.getDevice(record.local_id)`.
     - Если узел отсутствует: создается узел `ontology_storage.addDevice({record.local_id, record.name, ""})`.
     - Если узел существует: проверяется строгое совпадение имени `node.name == record.name`. При несовпадении выбрасывается `std::runtime_error` ("Graph and inventory disagree").
   - Проверка связей с возможностями (`HAS_FEATURE`):
     - Получение текущих связей устройства из онтологии (`ontology_storage.getDeviceAbilities(record.local_id)`).
     - Для каждой возможности из дескриптора драйвера:
       - Проверяется наличие записи в каталоге: `ontology_storage.getAbility(cap.getId())`. При отсутствии выбрасывается `std::runtime_error` ("Missing capability in ontology catalog").
       - Если связь с устройством отсутствует, вызывается `ontology_storage.addDeviceAbility(record.local_id, cap.getId())`.
     - Контроль лишних связей: если в онтологии присутствуют связи с возможностями, которых нет в дескрипторе драйвера, выбрасывается `std::runtime_error` ("Graph capabilities disagree with driver").
   - Существующие связи с комнатами (`LOCATED_IN`) не изменяются.
5. **Диагностика**:
   Фиксируется контекст с `device_id = record.local_id`, выводится сообщение на уровне `Debug`.
6. **Результат**:
   Возвращается собственная копия `DeviceRecord`.

### 3.2. Восстановление сохранённых устройств (`restoreStoredDevices`)
1. **Чтение инвентаря**:
   Получение всех записей `storage.getAll()`. Если список пуст, метод возвращает `0`.
2. **Фаза 1: Атомарная подготовка отсутствующих драйверов**:
   Создаётся временный контейнер `std::vector<std::unique_ptr<IDeviceDriver>> prepared_drivers`.
   Для каждой записи `record`:
   - Если устройство уже активно в `device_registry`, подготовка пропускается.
   - Иначе:
     - Проверяется фабрика `factory_registry.find(record.driver_id)`. Если отсутствует — `std::runtime_error` ("No factory registered for driver id").
     - Создаётся драйвер: `auto driver = factory->create(record)`. Если `nullptr` — `std::logic_error`.
     - Драйвер сохраняется в `prepared_drivers`.
   - **Гарантия**: сбой при подготовке любого драйвера отменяет всю операцию, ни один драйвер не попадает в `device_registry`, а онтология остаётся нетронутой.
3. **Фаза 2: Регистрация драйверов**:
   Все подготовленные драйверы последовательно перемещаются в `device_registry.add(...)`.
4. **Фаза 3: Актуализация онтологии**:
   Для ВСЕХ записей из хранилища (как вновь добавленных, так и ранее активных):
   - Получаем активный драйвер из реестра.
   - Вызываем `syncDeviceOntology(record, *driver)` (проверка/создание узла, проверка каталога возможностей, дополнение связей, контроль лишних связей).
5. **Диагностика**:
   Логируется сообщение на уровне `Info`: `"Stored devices restored."`.
6. **Результат**:
   Возвращается количество добавленных драйверов (`prepared_drivers.size()`).

## 4. Гарантии согласованности и обработка ошибок

- **Частичный сбой фабрики при регистрации**: запись остаётся в инвентаре, повторный вызов повторит создание драйвера.
- **Частичный сбой онтологии при регистрации/восстановлении**: добавленные драйверы сохраняются в реестре; повторный вызов досинхронизирует онтологию без дублирования драйверов.
- **Исключения**:
  - `UnsupportedDriverError` — неизвестная фабрика при регистрации (наследует `std::invalid_argument`).
  - `std::runtime_error` — неизвестная фабрика при восстановлении, отсутствие возможности в каталоге онтологии, несовпадение имени или лишние связи в графе.
  - `std::logic_error` — фабрика вернула `nullptr`.

## 5. Потокобезопасность и жизненный цикл

- Менеджер не содержит внутренних мьютексов и потоков; вызывающий код сериализует обращения.
- Деструктор тривиален и не выполняет сохранения на диск (сохранением управляет runtime).

## 6. Логирование и диагностика

- Диагностика осуществляется исключительно через фасад `smart_home::logging::log`.
- `restoreStoredDevices`: логирует факт успешного восстановления на уровне `Info`.
- `registerDiscoveredDevice`: логирует успешную регистрацию на уровне `Debug` с указанием `context.device_id`.
- Ошибки не логируются внутри менеджера повторно: исключения пробрасываются на границу обработки в вызывающий код.
