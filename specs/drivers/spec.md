# Драйверы устройств, протокольные создатели и каталог возможностей

ID: Drivers

Статус: проектируемый контракт универсального драйвера устройства, центрального каталога возможностей и функциональных создателей протоколов взамен жестких классов SimulatedLightDriver и SimulatedTemperatureSensorDriver.

## Цель

Связать цифровой двойник устройства (`Device` из модуля `devices`) с дескриптором поддерживаемых операций (`DeviceDescriptor` из `descriptors`), центральным каталогом стандартных возможностей (`CapabilityCatalog`) и протоколом физической связи (Zigbee2MQTT, симуляция в памяти, GPIO), обеспечивая динамическое добавление способностей.

## Требования

- DR-001: Универсальный драйвер `UniversalDeviceDriver` реализует `IDeviceDriver`, владеет экземпляром `Device` и управляет `DeviceDescriptor`. Жесткие классы-наследники `Light` и `TemperatureSensor` в драйверах отсутствуют.
- DR-002: Модуль предоставляет централизованный статический каталог стандартных возможностей `CapabilityCatalog` (`on_off`, `brightness`, `color`, `temperature`, `humidity`, `contact`, `battery`, `power`). Каждая возможность содержит готовый `CapabilityDescriptor`, операции (`*.set`, `*.get`) и связь со стандартными свойствами `PropertyTag` из модуля `devices`.
- DR-003: Драйвер поддерживает динамическое добавление способностей через метод `addCapability(const std::string &capability_id, ActionHandler handler = nullptr)`. При вызове:
  1. Описание возможности извлекается из `CapabilityCatalog`.
  2. Дескриптор устройства пополняется соответствующими операциями.
  3. В объекте `Device` регистрируются начальные свойства согласно дескриптору.
  4. Для операций записи регистрируется обработчик отправки команды в транспорт протокола (`ActionHandler`).
- DR-004: Создание драйверов протоколов выполняется через функциональные создатели (callables / lambdas):
  `using DeviceCreator = std::function<std::unique_ptr<IDeviceDriver>(const DeviceRecord &)>;`
  Строковый `driver_id` в `DeviceRecord` обозначает транспортный протокол (`"simulated"`, `"zigbee"`, `"gpio"`), а не конкретную модель оборудования.
- DR-005: Драйвер протокола `"zigbee"` связывает операции записи с публикацией сообщений в MQTT (`zigbee2mqtt/<physical_id>/set`). Операции чтения возвращают актуальные значения из цифрового двойника `Device` в RAM, обновляемого входящими сообщениями телеметрии.
- DR-006: Драйвер протокола `"simulated"` выполняет операции записи исключительно в оперативной памяти без сетевого и аппаратного ввода-вывода, обеспечивая воспроизводимое тестирование.
- DR-007: При холодном старте (`restoreStoredDevices`) драйвер устройства создаётся создателем соответствующего протокола с базовым `Device(local_id, name)`. Способности устройства восстанавливаются менеджером из связей онтологии (`ontology.getDeviceAbilities`) через `CapabilityCatalog`, обеспечивая полную доступность устройства в памяти до получения первого сетевого ответа от Zigbee2MQTT.

## Данные и дескрипторы

Поле `driver_id` в инвентаре `storage` и `DeviceRecord` хранит идентификатор протокола:
- `"simulated"` — виртуальные устройства;
- `"zigbee"` — устройства Zigbee2MQTT;
- `"gpio"` — прямые пины платы Raspberry Pi.

`physical_id` хранит идентификатор устройства внутри протокола:
- для Zigbee — IEEE-адрес (например, `"0x00158d0001d82345"`) или friendly name;
- для GPIO — номер пина (например, `"GPIO18"`);
- для симуляции — строковый идентификатор теста (например, `"SIM-LIGHT-001"`).

Стандартный состав каталога `CapabilityCatalog`:
- `on_off`: `on_off.set(bool)`, `on_off.get() -> bool`, свойство `props::light::State`
- `brightness`: `brightness.set(int [0..100])`, `brightness.get() -> int`, свойство `props::light::Brightness`
- `color`: `color.set(Color)`, `color.get() -> Color`, свойство `props::light::Rgb`
- `temperature`: `temperature.get() -> double`, свойство `props::climate::Temperature`
- `humidity`: `humidity.get() -> double`, свойство `props::climate::Humidity`
- `contact`: `contact.get() -> bool`, свойство `props::security::Contact`
- `battery`: `battery.get() -> int`, свойство `props::power::Battery`
- `power`: `power.get() -> double`, свойство `props::power::PowerWatts`

## Ошибки и инварианты

- Попытка добавить в драйвер возможность с идентификатором, отсутствующим в `CapabilityCatalog`, вызывает `std::invalid_argument`.
- Попытка повторно добавить уже зарегистрированную в драйвере возможность вызывает `std::invalid_argument`.
- Попытка создания драйвера с незарегистрированным `driver_id` вызывает `UnsupportedDriverError` (при регистрации) или `std::runtime_error` (при восстановлении).
- Передача `nullptr` вместо `Device` в драйвер запрещена (`std::invalid_argument`).
- Сетевая отправка команд протоколом не блокирует поток диспетчера команд.

## Границы

- Модуль связывает цифровой двойник `Device` с дескриптором и протоколом передачи команд.
- Модуль НЕ хранит персистентное состояние на SD-карте (ответственность `storage` и `persistence`).
- Модуль НЕ строит граф комнат онтологии (ответственность `ontology` и `device_management`).
- Модуль НЕ парсит низкоуровневые MQTT-пакеты (ответственность `mqtt`).

## Критерии приёмки

- Классы `SimulatedLightDriver`, `SimulatedLightDriverFactory`, `SimulatedTemperatureSensorDriver` удалены; их функциональность полностью воспроизводится через `UniversalDeviceDriver` и `CapabilityCatalog`.
- Создание устройства с произвольным набором способностей из каталога (например, лампа с датчиком температуры и батареей).
- Динамическое пополнение способностей работающего драйвера.
- Существующие тесты `CommandDispatcherTests`, `DeviceManagerTests`, `DeviceRegistryTests` компилируются и успешно завершаются.
