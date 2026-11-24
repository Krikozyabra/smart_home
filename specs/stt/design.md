# Технические решения модуля stt

Модуль реализован по [spec.md](spec.md) и `AGENTS.md`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), CMake 3.20+, C++17.

## 1. Цели и границы модуля

Модуль `stt` предоставляет локальное синхронное преобразование готового речевого аудиофрагмента в текст:
- **`ISttRecognizer`**: Абстрактный интерфейс распознавателя речи.
- **`SherpaSttRecognizer`**: Реализация на базе движка `sherpa-onnx` и модели `GigaAM-v3` Conformer-CTC.
- **`SttConfig`** и **`SttResult`**: Конфигурация и типизированный результат распознавания.

Модуль не выполняет непрерывный захват микрофона (это задача `audio`), детекцию ключевого слова активации (задача `oww`), семантический парсинг текста (NLU) или синтез речи (TTS).

## 2. Архитектура и зависимости

- Пространство имён: `smart_home::stt`.
- Публичный интерфейс C++17 в каталоге `stt/include/stt/`.
- Статическая библиотека `smart_home_stt` с alias `SmartHome::Stt`.
- Зависимости CMake:
  - `SmartHome::Logging` (PRIVATE) для диагностики.
  - `SmartHome::SherpaOnnx` (PRIVATE) для C API sherpa-onnx 2.0.1.
  - `SmartHome::OnnxRuntime` (PRIVATE) для проверок модели перед загрузкой в sherpa-onnx.
- Сборка STT включается через `SMART_HOME_BUILD_STT`. Сборка SDK через
  FetchContent использует тот же ONNX Runtime, что и OpenWakeWord.
- Сторонние типы `sherpa-onnx` и `onnxruntime` не выходят в публичные заголовки; полная изоляция через PIMPL (`std::unique_ptr<Impl>`).

## 3. Детальное проектирование компонентов

### 3.1. Конфигурация (`SttConfig.h`)

```cpp
namespace smart_home::stt {

struct SttConfig {
    std::string model_path;              ///< Путь к ONNX-модели GigaAM-v3 (INT8 или FP32).
    std::string tokens_path;             ///< Путь к словарю токенов (tokens.txt).
    int sample_rate_hz = 16000;         ///< Частота дискретизации входного сигнала (16000 Гц).
    int feature_dim = 64;               ///< Размерность Fbank признаков GigaAM-v3 (64).
    int num_threads = 2;                ///< Число потоков CPU инференса (1..4).
    std::string decoding_method = "greedy_search"; ///< Метод декодирования офлайн CTC.
};

void validateConfig(const SttConfig &config);

} // namespace smart_home::stt
```

Правила валидации `validateConfig()`:
- `model_path` и `tokens_path` указывают на обычные читаемые непустые файлы.
- `sample_rate_hz == 16000`.
- `feature_dim == 64`.
- `num_threads` находится в диапазоне 1..4.
- `decoding_method == "greedy_search"`. `modified_beam_search` в sherpa-onnx
  реализован для transducer-декодирования.
- При нарушении любого условия выбрасывается `std::invalid_argument` с информативным сообщением.

### 3.2. Результат распознавания (`SttResult.h`)

```cpp
namespace smart_home::stt {

struct SttResult {
    std::string text;                    ///< Распознанный текст на русском языке в кодировке UTF-8.
    float duration_seconds = 0.0f;       ///< Длительность распознанного аудиофрагмента в секундах.
    std::vector<std::string> tokens;     ///< Последовательность распознанных токенов/символов.
    std::vector<float> timestamps;       ///< Временные метки токенов в секундах от начала аудио.
};

} // namespace smart_home::stt
```

### 3.3. Публичный контракт (`ISttRecognizer.h`)

```cpp
namespace smart_home::stt {

class ISttRecognizer {
public:
    virtual ~ISttRecognizer() = default;

    virtual const SttConfig &getConfig() const noexcept = 0;

    virtual SttResult recognize(const std::int16_t *pcm_data, std::size_t sample_count) = 0;

    SttResult recognize(const std::vector<std::int16_t> &pcm_data) {
        return recognize(pcm_data.data(), pcm_data.size());
    }
};

} // namespace smart_home::stt
```

### 3.4. Реализация на базе sherpa-onnx (`SherpaSttRecognizer.h`)

```cpp
namespace smart_home::stt {

class SherpaSttRecognizer final : public ISttRecognizer {
public:
    explicit SherpaSttRecognizer(SttConfig config);
    ~SherpaSttRecognizer() override;

    SherpaSttRecognizer(const SherpaSttRecognizer &) = delete;
    SherpaSttRecognizer &operator=(const SherpaSttRecognizer &) = delete;
    SherpaSttRecognizer(SherpaSttRecognizer &&) noexcept;
    SherpaSttRecognizer &operator=(SherpaSttRecognizer &&) noexcept;

    const SttConfig &getConfig() const noexcept override;

    using ISttRecognizer::recognize;
    SttResult recognize(const std::int16_t *pcm_data, std::size_t sample_count) override;

private:
    struct Impl;
    SttConfig config_;
    std::unique_ptr<Impl> impl_;
};

} // namespace smart_home::stt
```

### 3.5. Внутреннее устройство `SherpaSttRecognizer::Impl`

- Владеет дескриптором `SherpaOnnxOfflineRecognizer`.
- Управляет переиспользуемым буфером нормализованных отсчётов. В начале
  резервируется место для одной секунды. Более длинный вход расширяет буфер.
- Конвертация `int16_t` -> `float`: `sample / 32768.0f`.
- Нулевой размер и `nullptr` дают пустой `SttResult{}`. Перед передачей в C API
  проверяется диапазон счётчика. Для короткого ненулевого входа используется
  дополнение тишиной с сохранением исходной длительности.
- Временная сессия ONNX Runtime проверяет сигнатуру, метаданные, словарь и форму
  фактических logits. Проверка нужна для ошибок модели и словаря, при которых
  sherpa-onnx может завершить процесс. Временная сессия освобождается до загрузки
  модели в sherpa-onnx.
- Конструктор прогревает полный тракт на 100 мс тишины. Такой вход даёт
  10 кадров Fbank до CTC subsampling и минимум два шага декодера. При каждом
  вызове создаётся новый поток офлайн-распознавания, использующий ту же сессию.
- Сессия, потоки и результаты C API освобождаются через RAII. Текст, токены и
  временные метки возвращаются собственными копиями.

## 4. Потоковая модель и ограничения памяти

- Экземпляр `SherpaSttRecognizer` не является потокобезопасным для одновременного параллельного вызова `recognize()` (stateful буферы). Вызовы должны быть последовательными, либо многопоточный код создаёт независимые экземпляры.
- Модуль не создаёт собственных фоновых потоков исполнения; инференс выполняется синхронно в вызывающем потоке с использованием пула `num_threads` внутри движка ONNX Runtime.
- Начальное значение для Raspberry Pi 4b составляет два потока. Значение
  RTF и расход памяти на плате измеряются отдельным запуском бенчмарка.
