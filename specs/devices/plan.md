# План реализации: Рефакторинг модуля Devices

Статус: проектируемый план рефакторинга модуля `SmartHome::Devices`.  
Основание: [spec.md](spec.md), [design.md](design.md), `AGENTS.md`, `specs/drivers/spec.md`, `specs/persistence/spec.md`.  
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), сборка CMake 3.20+, C++17.

---

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **DV-001** | `Device(DeviceId, std::string)` с идентификатором и именем. Move-only (копирование запрещено). Методы `getId()`, `getName()`, `setName()`. | `Device.h`, `Device.cpp` с `delete` copy constructor/assignment | `tests/DeviceTests.cpp` (проверка инициализации, перемещения, запрета копирования) |
| **DV-002** | Динамический словарь свойств `unordered_map<string, Value>`. Защита от пустых ключей (`invalid_argument`). Запись заменяет значение. | `Device::setProperty()`, `Device::getProperty()`, `Device::getProperties()` | `tests/DeviceTests.cpp` (проверка CRUD операций над свойствами, проверка пустых ключей) |
| **DV-003** | `PropertyTag<T>` конструируется во время компиляции (`constexpr`) без аллокаций. Связывает ключ, тип C++, границы `min`/`max` и `unit`. Только типы из `Value`. | Шаблон `PropertyTag<T>` и специализации в `PropertyTag.h` | Проверка `static_assert`, тесты `constexpr` в `tests/DeviceTests.cpp` |
| **DV-004** | `set(tag, value)` проверяет диапазон `[min; max]`. Выход за границы вызывает `invalid_argument` со строгой гарантией безопасности (словарь не меняется). | Шаблонный метод `Device::set` в `Device.h` | `tests/DeviceTests.cpp` (проверка валидации диапазонов и сохранения исходного значения при сбое) |
| **DV-005** | `get(tag)` возвращает `std::optional<T>`. При отсутствии ключа или несовпадении типа возвращает `nullopt` без исключений. `has(tag)` возвращает bool. | Шаблонные методы `Device::get` и `Device::has` в `Device.h` | `tests/DeviceTests.cpp` (проверка возврата `nullopt` для отсутствующих свойств и при несовпадении типов) |
| **DV-006** | Невладеющие представления (`OnOffView`, `BrightnessView`, `ColorView`, `TemperatureView`) адаптируют `Device&` к интерфейсам возможностей. Чтение отсутствующего свойства даёт `logic_error`. | Классы в `devices/include/devices/views/` | `tests/DeviceTests.cpp`, `tests/DeviceManagerTests.cpp`, `execution` обработчики |
| **DV-007** | Драйверы создают универсальный `Device` и наполняют свойства через `PropertyTag`. Удаление жестких классов-наследников `Light` и `TemperatureSensor`. | `SimulatedLightDriver.cpp`, `SimulatedTemperatureSensorDriver.cpp` | `tests/FactoriesTests.cpp`, удаление файлов `Light.*` и `TemperatureSensor.*` |

---

## 2. Пошаговая структура работ (Work Breakdown Structure)

### Шаг 1: Перенос `Value` и реализация `PropertyTag<T>`
- [ ] 1.1. Создать `devices/include/devices/types/Value.h`:
  - Определение `using Value = std::variant<bool, int, double, char, Color, std::string>;`.
  - Doxygen-документация на русском языке.
- [ ] 1.2. Обновить `command/include/command/Value.h`:
  - Включить `devices/types/Value.h` для сохранения 100% обратной совместимости.
- [ ] 1.3. Реализовать `devices/include/devices/PropertyTag.h`:
  - Шаблон `PropertyTag<T>` для арифметических типов (`int`, `double`, `char`, `bool`) с полями `has_min`, `min_value`, `has_max`, `max_value`, `unit`.
  - `constexpr` конструкторы и метод `validate(const T &val) const noexcept`.
  - Специализации для `std::string` и `Color` для соблюдения константности `constexpr` в стандарте C++17.
  - `static_assert` на допустимость типа `T` в `Value`.
- [ ] 1.4. Создать `devices/include/devices/PropertyTags.h`:
  - Определение стандартных свойств с ключевым словом `inline constexpr` в пространствах `props::light`, `props::climate`, `props::security`, `props::power` в точном соответствии со спецификацией [spec.md](spec.md#L26-L45).

### Шаг 2: Универсализация класса `Device`
- [ ] 2.1. Модифицировать `devices/include/devices/Device.h`:
  - Удалить наследование, сделать класс финальным/автономным.
  - Запретить копирование: `Device(const Device&) = delete; Device& operator=(const Device&) = delete;`.
  - Разрешить перемещение: `Device(Device&&) noexcept = default; Device& operator=(Device&&) noexcept = default;`.
  - Добавить приватный словарь `std::unordered_map<std::string, Value> properties_`.
  - Объявить сырые методы: `setProperty`, `getProperty`, `hasProperty`, `removeProperty`, `getProperties`.
  - Объявить шаблонные методы: `set<T>`, `get<T>`, `has<T>`, `view<ViewType>()`.
- [ ] 2.2. Реализовать методы в `devices/src/Device.cpp`:
  - Проверка `key.empty()` с выбросом `std::invalid_argument` во всех сырых методах.
  - Проверка `name.empty()` в `setName`.
- [ ] 2.3. Обновить `devices/CMakeLists.txt`:
  - Подключить заголовочные каталоги и исходные файлы.

### Шаг 3: Реализация невладеющих представлений возможностей (Views)
- [ ] 3.1. Создать каталог `devices/include/devices/views/`.
- [ ] 3.2. Реализовать `devices/include/devices/views/OnOffView.h`:
  - Наследование от `IOnOff`.
  - Хранение `Device &device_`.
  - Реализация `isOn()` (чтение `props::light::State`, выброс `std::logic_error` при отсутствии) и `setOn(bool)`.
- [ ] 3.3. Реализовать `devices/include/devices/views/BrightnessView.h`:
  - Наследование от `IBrightness`.
  - Хранение `Device &device_`.
  - Реализация `getBrightness()` (чтение `props::light::Brightness`) и `setBrightness(uint8_t)`.
- [ ] 3.4. Реализовать `devices/include/devices/views/ColorView.h`:
  - Наследование от `IColor`.
  - Хранение `Device &device_`.
  - Реализация `getColor()` и `setColor(Color)`.
- [ ] 3.5. Реализовать `devices/include/devices/views/TemperatureView.h`:
  - Наследование от `ITemperature`.
  - Хранение `const Device &device_`.
  - Реализация `getTemperature()`.

### Шаг 4: Обновление драйверов и обработчиков команд
- [ ] 4.1. Обновить `drivers/src/SimulatedLightDriver.cpp`:
  - Заменить создание `Light` на создание универсального `Device(c_local_id, c_name)`.
  - Наполнить свойства через `device->set(props::light::State, c_isOn)`, `device->set(props::light::Brightness, c_brightness)`, `device->set(props::light::Rgb, c_color)`.
  - Удалить включение `devices/Light.h`.
- [ ] 4.2. Обновить `drivers/src/SimulatedTemperatureSensorDriver.cpp`:
  - Заменить создание `TemperatureSensor` на `Device`.
  - Наполнить свойство `device->set(props::climate::Temperature, c_temperature)`.
  - Удалить включение `devices/TemperatureSensor.h`.
- [ ] 4.3. Обновить обработчики в `execution/src/handlers/`:
  - В `OnOffHandlers.cpp`: заменить `dynamic_cast<IOnOff*>(&device)` на создание стекового `OnOffView view(device);`.
  - В `BrightnessHandlers.cpp`: заменить `dynamic_cast<IBrightness*>(&device)` на `BrightnessView view(device);`.
  - В `ColorHandlers.cpp`: заменить `dynamic_cast<IColor*>(&device)` на `ColorView view(device);`.
  - В `TemperatureHandlers.cpp`: заменить `dynamic_cast<const ITemperature*>(&device)` на `TemperatureView view(device);`.
- [ ] 4.4. Удалить устаревшие файлы:
  - `devices/include/devices/Light.h`, `devices/src/Light.cpp`.
  - `devices/include/devices/TemperatureSensor.h`, `devices/src/TemperatureSensor.cpp`.
  - Исключить их из `devices/CMakeLists.txt`.

### Шаг 5: Модульные тесты и верификация
- [ ] 5.1. Создать модульный тест `tests/DeviceTests.cpp`:
  - Проверка инициализации `Device(id, name)`, геттеров и сеттеров.
  - Проверка перемещения объекта устройства и отсутствия копирования.
  - Проверка сырого доступа к словарю (`setProperty`, `getProperty`, `removeProperty`).
  - Проверка выброса `invalid_argument` при пустом ключе свойства.
  - Проверка типобезопасного `set` и `get` через `PropertyTag`.
  - Проверка валидации диапазона `min`/`max` и строгой гарантии безопасности исключений.
  - Проверка возврата `std::nullopt` для отсутствующих свойств и несовпадающих типов.
  - Проверка работы всех представлений (`OnOffView`, `BrightnessView`, `ColorView`, `TemperatureView`).
  - Проверка выброса `std::logic_error` при обращении через View к устройству без требуемого свойства.
- [ ] 5.2. Обновить существующий тест `tests/DeviceManagerTests.cpp`:
  - Заменить `dynamic_cast<IBrightness&>` и `dynamic_cast<IOnOff&>` на `BrightnessView` и `OnOffView`.
- [ ] 5.3. Зарегистрировать `device_tests` в `tests/CMakeLists.txt`.
- [ ] 5.4. Запустить полный цикл компиляции и CTest.

---

## 3. План верификации

### 3.1. Автоматизированные тесты
```sh
# Сборка проекта и тестов устройств
cmake -B build -S . -DSMART_HOME_RUNTIME_TESTING=1 -DBUILD_DOCS=ON
cmake --build build --target device_tests command_dispatcher_tests device_manager_tests factories_tests -j4

# Запуск модульного теста устройств
ctest --test-dir build -R device_tests -V

# Запуск полного набора CTest
ctest --test-dir build --output-on-failure

# Проверка документации Doxygen
cmake --build build --target docs
```

### 3.2. Критерии успешности проверки
1. Модульный тест `device_tests` полностью проходит (все проверки `DV-001`–`DV-006`).
2. Все существующие тесты диспетчеризации (`command_dispatcher_tests`), менеджера устройств (`device_manager_tests`), фабрик (`factories_tests`) и интеграционные тесты рантайма (`runtime_*_tests`) проходят без ошибок (100% pass).
3. В кодовой базе отсутствуют включения устаревших `Light.h` и `TemperatureSensor.h`.
4. Сборка документации Doxygen завершается без предупреждений.
