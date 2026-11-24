# План реализации модуля stt

Основание — [spec.md](spec.md), [design.md](design.md), `AGENTS.md`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), CMake 3.20+, C++17.

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **STT-001** | Конфигурация и валидация `SttConfig` | `SttConfig.h`, `validateConfig()` | Юнит-тесты валидации путей, частоты дискретизации, числа потоков |
| **STT-002** | Формат входных аудиоданных PCM16 | `ISttRecognizer::recognize()`, нормализация в `[-1.0, 1.0]` | Тесты корректности масштабирования отсчётов, проверка нулевых/пустых буферов |
| **STT-003** | Акустический инференс и CTC декодирование | `SherpaSttRecognizer::Impl`, интеграция с `sherpa-onnx` | Тест пайплайна распознавания на тестовой модели и эталонном аудио |
| **STT-004** | Формирование `SttResult` | Структура `SttResult` с текстом, длительностью, токенами | Проверка полей результата, кодировки UTF-8 и расчёта длительности |
| **STT-005** | Прогрев и жизненный цикл ресурсов | Загрузка и прогрев в конструкторе, RAII-деструктор | Многократное распознавание, передача владения и проверка санитайзером |
| **STT-006** | Обработка ошибок и устойчивость | Трансляция ошибок SDK в `std::runtime_error` / `invalid_argument` | Негативные тесты повреждённых файлов, невалидных конфигураций |

## 2. Пошаговая структура работ (Work Breakdown Structure)

### Шаг 1: Инфраструктура CMake и SDK
- [x] 1.1. Создать [cmake/SherpaOnnx.cmake](../../cmake/SherpaOnnx.cmake) для поиска установленного `sherpa-onnx` и опциональной сборки через `FetchContent` (`SMART_HOME_FETCH_SHERPA_ONNX`).
- [x] 1.2. Создать каталог `stt/` со структурой `include/stt/` и `src/`.
- [x] 1.3. Создать [stt/CMakeLists.txt](../../stt/CMakeLists.txt) с таргетом `SmartHome::Stt` (C++17, `-Wall -Wextra -Wpedantic`).
- [x] 1.4. Создать [stt/DownloadModels.cmake](../../stt/DownloadModels.cmake) с кастомной целью `stt_models` для опционального скачивания модели `gigaam_v3_ctc_int8.onnx` и `tokens.txt`.
- [x] 1.5. Подключить `stt` в корневом [CMakeLists.txt](../../CMakeLists.txt).

### Шаг 2: Реализация конфигурации и структур данных
- [x] 2.1. Создать [stt/include/stt/SttConfig.h](../../stt/include/stt/SttConfig.h) и [stt/src/SttConfig.cpp](../../stt/src/SttConfig.cpp) с реализацией `validateConfig()`.
- [x] 2.2. Создать [stt/include/stt/SttResult.h](../../stt/include/stt/SttResult.h).

### Шаг 3: Интерфейс и реализация распознавателя речи
- [x] 3.1. Создать интерфейс [stt/include/stt/ISttRecognizer.h](../../stt/include/stt/ISttRecognizer.h).
- [x] 3.2. Создать заголовок [stt/include/stt/SherpaSttRecognizer.h](../../stt/include/stt/SherpaSttRecognizer.h).
- [x] 3.3. Создать [stt/src/SherpaSttRecognizer.cpp](../../stt/src/SherpaSttRecognizer.cpp) и проверить следующие операции.
  - Обернуть дескрипторы `sherpa-onnx` в безопасные RAII-контейнеры.
  - Реализовать нормализацию PCM16 в переиспользуемый вектор `float`.
  - Реализовать CTC-декодирование и сборку `SttResult`.
  - Реализовать однократный прогрев пайплайна в конструкторе.

### Шаг 4: Модульные и интеграционные тесты
- [x] 4.1. Создать синтетический тестовый стенд (токены и компактный тестовый ONNX граф) в `tests/stt/`.
- [x] 4.2. Создать [tests/SttTests.cpp](../../tests/SttTests.cpp) с проверками конфигурации, аудио и ресурсов.
  - Тесты валидации конфигурации (пустые пути, невалидная частота, число потоков).
  - Тесты обработки нулевых, пустых и коротких аудиовекторов.
  - Тесты жизненного цикла и последовательных вызовов (`lifetimeAndSoak`).
- [x] 4.3. Создать [tests/SttModelTests.cpp](../../tests/SttModelTests.cpp) для опционального бенчмарка реальной модели GigaAM-v3 на реальном аудио и вычисления RTF (Real Time Factor) на Raspberry Pi 4.
- [x] 4.4. Зарегистрировать тесты `stt_tests` и `stt_model_tests` в [tests/CMakeLists.txt](../../tests/CMakeLists.txt).

### Шаг 5: Верификация и документация
- [x] 5.1. Сборка проекта и прохождение тестов `stt_tests` через CTest.
- [x] 5.2. Проверка регрессии 35 существующих тестов репозитория.
- [x] 5.3. Сборка документации Doxygen (`cmake --build build --target docs`).
- [x] 5.4. Фиксация результатов в [walkthrough.md](walkthrough.md).
