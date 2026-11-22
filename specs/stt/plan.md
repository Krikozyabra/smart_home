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
| **STT-005** | Прогрев и жизненный цикл ресурсов | Загрузка и прогрев в конструкторе, RAII-деструктор | Тесты отсутствия утечек памяти, тест последовательных повторных вызовов |
| **STT-006** | Обработка ошибок и устойчивость | Трансляция ошибок SDK в `std::runtime_error` / `invalid_argument` | Негативные тесты повреждённых файлов, невалидных конфигураций |

## 2. Пошаговая структура работ (Work Breakdown Structure)

### Шаг 1: Инфраструктура CMake и SDK
- [ ] 1.1. Создать [cmake/SherpaOnnx.cmake](../../cmake/SherpaOnnx.cmake): поиск установленного `sherpa-onnx` или опциональная сборка через `FetchContent` (`SMART_HOME_FETCH_SHERPA_ONNX`).
- [ ] 1.2. Создать каталог `stt/` со структурой `include/stt/` и `src/`.
- [ ] 1.3. Создать [stt/CMakeLists.txt](../../stt/CMakeLists.txt) с таргетом `SmartHome::Stt` (C++17, `-Wall -Wextra -Wpedantic`).
- [ ] 1.4. Создать [stt/DownloadModels.cmake](../../stt/DownloadModels.cmake) с кастомной целью `stt_models` для опционального скачивания модели `gigaam_v3_ctc_int8.onnx` и `tokens.txt`.
- [ ] 1.5. Подключить `stt` в корневом [CMakeLists.txt](../../CMakeLists.txt).

### Шаг 2: Реализация конфигурации и структур данных
- [ ] 2.1. Создать [stt/include/stt/SttConfig.h](../../stt/include/stt/SttConfig.h) и [stt/src/SttConfig.cpp](../../stt/src/SttConfig.cpp) с реализацией `validateConfig()`.
- [ ] 2.2. Создать [stt/include/stt/SttResult.h](../../stt/include/stt/SttResult.h).

### Шаг 3: Интерфейс и реализация распознавателя речи
- [ ] 3.1. Создать интерфейс [stt/include/stt/ISttRecognizer.h](../../stt/include/stt/ISttRecognizer.h).
- [ ] 3.2. Создать заголовок [stt/include/stt/SherpaSttRecognizer.h](../../stt/include/stt/SherpaSttRecognizer.h).
- [ ] 3.3. Создать [stt/src/SherpaSttRecognizer.cpp](../../stt/src/SherpaSttRecognizer.cpp):
  - Обернуть дескрипторы `sherpa-onnx` в безопасные RAII-контейнеры.
  - Реализовать нормализацию PCM16 в переиспользуемый вектор `float`.
  - Реализовать CTC-декодирование и сборку `SttResult`.
  - Реализовать однократный прогрев пайплайна в конструкторе.

### Шаг 4: Модульные и интеграционные тесты
- [ ] 4.1. Создать синтетический тестовый стенд (токены и компактный тестовый ONNX граф) в `tests/stt/`.
- [ ] 4.2. Создать [tests/SttTests.cpp](../../tests/SttTests.cpp):
  - Тесты валидации конфигурации (пустые пути, невалидная частота, число потоков).
  - Тесты обработки нулевых, пустых и коротких аудиовекторов.
  - Тесты жизненного цикла и последовательных вызовов (`lifetimeAndSoak`).
- [ ] 4.3. Создать [tests/SttModelTests.cpp](../../tests/SttModelTests.cpp) для опционального бенчмарка реальной модели GigaAM-v3 на реальном аудио и вычисления RTF (Real Time Factor) на Raspberry Pi 4.
- [ ] 4.4. Зарегистрировать тесты `stt_tests` и `stt_model_tests` в [tests/CMakeLists.txt](../../tests/CMakeLists.txt).

### Шаг 5: Верификация и документация
- [ ] 5.1. Сборка проекта и прохождение тестов `stt_tests` через CTest.
- [ ] 5.2. Проверка регрессии существующих 34 тестов репозитория.
- [ ] 5.3. Сборка Doxygen документации (`cmake --build build --target docs`).
- [ ] 5.4. Фиксация результатов в `walkthrough.md`.
