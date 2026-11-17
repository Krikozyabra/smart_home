# Архитектурный дизайн: Модуль Devices

ID: Devices-Design

## Архитектура и структура классов

```mermaid
classDiagram
    class PropertyTag~T~ {
        +const char* key
        +optional~T~ min_value
        +optional~T~ max_value
        +const char* unit
    }

    class Value {
        <<variant>>
        bool, int, double, char, Color, string
    }

    class Device {
        -DeviceId id
        -string name
        -unordered_map~string, Value~ properties
        +getId() DeviceId
        +getName() string
        +setName(string)
        +setProperty(string, Value)
        +getProperty(string) optional~Value~
        +hasProperty(string) bool
        +removeProperty(string) bool
        +set~T~(PropertyTag~T~, T)
        +get~T~(PropertyTag~T~) optional~T~
        +has~T~(PropertyTag~T~) bool
        +view~V~() V
    }

    class IOnOff {
        <<interface>>
        +isOn()* bool
        +setOn(bool)*
    }

    class IBrightness {
        <<interface>>
        +getBrightness()* uint8_t
        +setBrightness(uint8_t)*
    }

    class ITemperature {
        <<interface>>
        +getTemperature()* double
    }

    class OnOffView {
        -Device& device
        +isOn() bool
        +setOn(bool)
    }

    class BrightnessView {
        -Device& device
        +getBrightness() uint8_t
        +setBrightness(uint8_t)
    }

    class TemperatureView {
        -const Device& device
        +getTemperature() double
    }

    IOnOff <|.. OnOffView
    IBrightness <|.. BrightnessView
    ITemperature <|.. TemperatureView

    OnOffView o-- Device : non-owning ref
    BrightnessView o-- Device : non-owning ref
    TemperatureView o-- Device : non-owning ref

    Device ..> PropertyTag : uses
    Device *-- Value : contains
```

## Жизненный цикл и интеграция компонентов

1. **Создание устройства драйвером**:
   - Драйвер (`SimulatedLightDriver`) создает экземпляр `Device(local_id, name)`.
   - Через `device.set(props::light::State, false)`, `device.set(props::light::Brightness, 50)` драйвер наполняет устройство начальным состоянием.
   - Драйвер формирует `DeviceDescriptor`, описывающий поддерживаемые операции (`on_off.set`, `on_off.get`, `brightness.set`, `brightness.get`).

2. **Диспетчеризация команд (`execution`)**:
   - Обработчик команды (`OnOffHandlers.cpp`) получает ссылку `Device&` от драйвера.
   - Вместо вызова `dynamic_cast<IOnOff*>(&device)` обработчик обращается к представлению:
     ```cpp
     OnOffView view(device);
     view.setOn(command_argument_value);
     ```
     или напрямую к типизированному свойству:
     ```cpp
     if (!device.has(props::light::State))
         throw std::logic_error("Device does not support on_off.state");
     device.set(props::light::State, command_argument_value);
     ```

3. **Связь с Онтологией и Инвентарем**:
   - Онтология хранит связи топологии (`HAS_FEATURE -> on_off`, `HAS_FEATURE -> brightness`).
   - Имена свойств `props::light::State.key` (`"on_off.state"`) согласованы с префиксами возможностей онтологии (`"on_off"`).
   - В SQLite сохраняются пары `(local_id, property_key, value)`.

## Потоковая модель и ограничения памяти на Raspberry Pi 4

- `Device` не содержит собственных мьютексов и потоков; доступ к объекту устройства сериализуется на уровне драйвера или вызывающего цикла `runtime`.
- Экземпляры `PropertyTag<T>` являются `constexpr` и в скомпилированном бинарном файле занимают место в секции `.rodata` без выделений памяти в куче.
- Метод `view<T>()` создает временную обертку на стеке с размером в один указатель (`sizeof(void*) = 8` байт на 64-битном ARMv8 / Raspberry Pi OS 64-bit).
