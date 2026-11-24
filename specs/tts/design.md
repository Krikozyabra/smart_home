# Технические решения модуля tts

Модуль проектируется по [spec.md](spec.md) и `AGENTS.md`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), CMake 3.20+, C++17.

## 1. Цели и границы модуля

Модуль `tts` предоставляет локальное синхронное преобразование текстовой строки на русском языке в аудиосигнал:
- **`ITtsSynthesizer`**: Абстрактный интерфейс синтезатора речи.
- **`SileroTtsSynthesizer`**: Реализация на базе движка ONNX Runtime и русскоязычной модели Silero TTS.
- **`TtsConfig`**: Конфигурация синтезатора и валидация параметров.
- **`TtsAudio`**: Типизированный результат синтеза (отсчёты `float` и `PCM16`, частота дискретизации, длительность).

Модуль не выполняет прямое воспроизведение звука через динамик/ALSA (это задача тракта вывода звука), захват микрофона (`audio`), детекцию ключевого слова (`oww`), распознавание речи (`stt`) или семантический парсинг текста (NLU).

## 2. Архитектура и зависимости

- Пространство имён: `smart_home::tts`.
- Публичный интерфейс C++17 в каталоге `tts/include/tts/`.
- Статическая библиотека `smart_home_tts` с alias `SmartHome::Tts`.
- Зависимости CMake:
  - `SmartHome::Logging` (PRIVATE) для диагностики.
  - `SmartHome::OnnxRuntime` (PRIVATE) для C++ API ONNX Runtime.
- Сборка TTS включается опцией `SMART_HOME_BUILD_TTS` (по умолчанию `OFF`).
- Сторонние типы `onnxruntime` не выходят в публичные заголовки; полная изоляция через PIMPL (`std::unique_ptr<Impl>`).

## 3. Детальное проектирование компонентов

### 3.1. Конфигурация (`TtsConfig.h`)

```cpp
namespace smart_home::tts {

struct TtsConfig {
    std::string model_path;          ///< Путь к ONNX-модели Silero TTS; непустой.
    std::string tokens_path;         ///< Путь к словарю символов; пустой = встроенная таблица символов.
    int sample_rate_hz = 24000;      ///< Частота дискретизации (8000, 24000, 48000 Гц).
    std::string speaker = "xenia";   ///< Имя диктора (aidar, baya, kseniya, xenia, eugene, random).
    int num_threads = 2;             ///< Число потоков CPU инференса (1..4).
};

void validateConfig(const TtsConfig &config);

} // namespace smart_home::tts
```

Правила валидации `validateConfig()`:
- `model_path` указывает на обычный читаемый непустой файл.
- `tokens_path` при непустом значении указывает на обычный читаемый непустой файл.
- `sample_rate_hz` принадлежит множеству `{8000, 24000, 48000}`.
- `speaker` принадлежит списку `{"aidar", "baya", "kseniya", "xenia", "eugene", "random"}`.
- `num_threads` находится в диапазоне `1..4`.
- При нарушении выбрасывается `std::invalid_argument`.

Внешний словарь содержит один Unicode-символ на строку в UTF-8. Позиция строки
начиная с нуля задаёт индекс символа. Пробел записывается как строка из одного
пробела. Поддерживаются BOM в начале и окончания строк LF или CRLF. Повторные
символы и пустые строки отклоняются. Словарь должен содержать пробел, `+`,
`.` и `~`. Индексы должны соответствовать графу, заданному в `model_path`.

### 3.2. Результат синтеза (`TtsAudio.h`)

```cpp
namespace smart_home::tts {

struct TtsAudio {
    std::vector<float> samples_float;        ///< Нормализованные отсчёты в диапазоне [-1.0, 1.0].
    std::vector<std::int16_t> samples_pcm16;  ///< Отсчёты в формате signed PCM16 (порядок хоста).
    int sample_rate_hz = 0;                  ///< Частота дискретизации сигнала (Гц).
    float duration_seconds = 0.0f;           ///< Длительность аудиофрагмента в секундах.

    bool empty() const noexcept {
        return samples_float.empty() && samples_pcm16.empty();
    }
};

} // namespace smart_home::tts
```

### 3.3. Публичный контракт (`ITtsSynthesizer.h`)

```cpp
namespace smart_home::tts {

class ITtsSynthesizer {
public:
    virtual ~ITtsSynthesizer() = default;

    virtual const TtsConfig &getConfig() const noexcept = 0;
    virtual TtsAudio synthesize(std::string_view text) = 0;

    TtsAudio synthesize(const std::string &text) {
        return synthesize(std::string_view(text));
    }
};

} // namespace smart_home::tts
```

Перегрузка `synthesize(const char *)` принимает строки с завершающим нулевым
байтом, включая литералы, и отклоняет `nullptr`.

### 3.4. Реализация на базе ONNX Runtime (`SileroTtsSynthesizer.h`)

```cpp
namespace smart_home::tts {

class SileroTtsSynthesizer final : public ITtsSynthesizer {
public:
    explicit SileroTtsSynthesizer(TtsConfig config);
    ~SileroTtsSynthesizer() override;

    SileroTtsSynthesizer(const SileroTtsSynthesizer &) = delete;
    SileroTtsSynthesizer &operator=(const SileroTtsSynthesizer &) = delete;
    SileroTtsSynthesizer(SileroTtsSynthesizer &&) noexcept;
    SileroTtsSynthesizer &operator=(SileroTtsSynthesizer &&) noexcept;

    const TtsConfig &getConfig() const noexcept override;
    using ITtsSynthesizer::synthesize;
    TtsAudio synthesize(std::string_view text) override;

private:
    struct Impl;
    TtsConfig config_;
    std::unique_ptr<Impl> impl_;
};

} // namespace smart_home::tts
```

### 3.5. Внутреннее устройство `SileroTtsSynthesizer::Impl`

- Владеет `Ort::Env` и `Ort::Session`.
- Порядок встроенного алфавита:
  `_~абвгдеёжзийклмнопрстуфхцчшщъыьэюя +.,!?…:;–` (0: padding, 1: end token `~`).
- Токенизация входной строки:
  - Строгое декодирование UTF-8 и приведение русских и ASCII-букв к нижнему регистру.
  - Поддержка символа `+` перед ударными гласными для управления ударением.
  - Фильтрация символов, отсутствующих в словаре, и зарезервированных `_` и `~`.
  - Обрезка пробелов по краям и сжатие последовательностей пробельных символов Unicode.
  - Добавление завершающей точки при отсутствии терминальной пунктуации (`.`, `!`, `?`).
  - Добавление символа окончания последовательности `~`.
  - Преобразование в числовые индексы токенов `std::vector<int64_t>`.
- Входные тензоры ONNX Runtime:
  - `input`: `[1, sequence_length]` (`int64`, переменная длина последовательности).
  - `speaker`: `[1]` (`int64`, индексы 0–5 соответствуют `aidar`, `baya`, `kseniya`,
    `xenia`, `eugene`, `random`).
  - `sample_rate`: `[1]` (`int64`, целевая частота).
- Выходные данные и конвертация:
  - Модель возвращает `output`: `[1, num_samples]` (`float`).
  - Конечные отсчёты ограничиваются диапазоном `[-1, 1]` и сохраняются в `samples_float`.
  - Отсчёты PCM16 вычисляются из этих значений как `round(sample_float * 32767.0f)`.
- Обработка краевых случаев:
  - Ввод без токенов возвращает `TtsAudio{}` без аллокации тензоров и инференса.
- Прогрев:
  - Конструктор выполняет синтез короткого проверочного слова (например, `"проверка"`) для инициализации внутренних структур ONNX Runtime.

## 4. Потоковая модель и ограничения памяти

- Экземпляр `SileroTtsSynthesizer` не является потокобезопасным для параллельного вызова `synthesize()`. Обращения должны быть последовательными, либо многопоточный код создаёт независимые экземпляры.
- Инференс выполняется синхронно в вызывающем потоке с использованием пула `num_threads` внутри движка ONNX Runtime.
- По умолчанию используются 2 потока CPU на Raspberry Pi 4b.

## 5. Подготовка модели

Для набора дикторов и частот из спецификации выбран официальный Silero `v4_ru`.
Он опубликован в формате PyTorch. Отдельная цель `tts_models` проверяет SHA-256
архива `v4_ru.pt` и выполняет локальный экспорт графа в один файл `silero_ru.onnx`.
Источником весов служит `https://models.silero.ai/models/tts/ru/v4_ru.pt` с SHA-256
`896ab96347d5bd781ab97959d4fd6885620e5aab52405d3445626eb7c1414b00`.
Условия использования модели приведены в [лицензии Silero](https://github.com/snakers4/silero-models/blob/master/LICENSE).

Экспорт сопоставляет индексы встроенного алфавита с таблицей `v4_ru`, добавляет
служебный начальный токен и использует ветви вокодера для всех трёх частот.
Диктор `random` получает новые случайные эмбеддинги на каждом вызове.
Сопоставление отсчётов с исходным `v4_ru` проверяется для текста с ручной
разметкой и двух фиксированных голосов. Автоматическая расстановка ударений,
подстановка `ё` и обработка SSML остаются за пределами экспорта.

`tts_models` также принимает URL готового совместимого ONNX-файла с заданным
SHA-256. Подготовка модели выполняется отдельной командой. Для работы на
целевой плате достаточно готового файла и ONNX Runtime.
