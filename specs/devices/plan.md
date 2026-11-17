# План реализации: Рефакторинг модуля Devices

ID: Devices-Plan

## Структура работ (Work Breakdown Structure)

### Этап 1: Перенос `Value` и внедрение `PropertyTag`
1. Создать `devices/include/devices/types/Value.h` с определением `Value = std::variant<bool, int, double, char, Color, std::string>`.
2. Обновить `command/include/command/Value.h` для включения `devices/types/Value.h` (обратная совместимость).
3. Создать `devices/include/devices/PropertyTag.h` с шаблоном `PropertyTag<T>`.
4. Создать `devices/include/devices/PropertyTags.h` с пространствами `props::light`, `props::climate`, `props::security`, `props::power`.

### Этап 2: Универсализация класса `Device`
1. Обновить `devices/include/devices/Device.h`:
   - Добавить хранилище `std::unordered_map<std::string, Value> properties`.
   - Добавить сырые методы `setProperty`, `getProperty`, `hasProperty`, `removeProperty`, `getProperties`.
   - Добавить шаблонные методы `set`, `get`, `has`, `view`.
2. Реализовать методы в `devices/src/Device.cpp`.

### Этап 3: Создание представлений возможностей (Views)
1. Создать `devices/include/devices/views/OnOffView.h` (реализует `IOnOff`).
2. Создать `devices/include/devices/views/BrightnessView.h` (реализует `IBrightness`).
3. Создать `devices/include/devices/views/ColorView.h` (реализует `IColor`).
4. Создать `devices/include/devices/views/TemperatureView.h` (реализует `ITemperature`).

### Этап 4: Обновление драйверов и обработчиков команд
1. Обновить `SimulatedLightDriver.cpp` и `SimulatedTemperatureSensorDriver.cpp`:
   - Создавать `Device` напрямую, наполняя свойства через `PropertyTag`.
   - Удалить включение и использование `Light.h` и `TemperatureSensor.h`.
2. Обновить обработчики в `execution/src/handlers/`:
   - `OnOffHandlers.cpp`: использовать `OnOffView` или `device.set(props::light::State, ...)`.
   - `BrightnessHandlers.cpp`: использовать `BrightnessView` или `device.set(props::light::Brightness, ...)`.
   - `ColorHandlers.cpp`: использовать `ColorView` или `device.set(props::light::Rgb, ...)`.
   - `TemperatureHandlers.cpp`: использовать `TemperatureView` или `device.get(props::climate::Temperature)`.
3. Удалить устаревшие файлы `Light.h`, `Light.cpp`, `TemperatureSensor.h`, `TemperatureSensor.cpp` и исключить их из `devices/CMakeLists.txt`.

### Этап 5: Обновление и добавление тестов
1. Создать `tests/DeviceTests.cpp` для тестирования `Device`, `PropertyTag`, валидации `min`/`max` и представлений `Views`.
2. Обновить `tests/DeviceManagerTests.cpp` (заменить `dynamic_cast` на `BrightnessView`/`OnOffView`).
3. Зарегистрировать `DeviceTests` в `tests/CMakeLists.txt`.
4. Запустить все тесты CTest и убедиться в 100% прохождении.
