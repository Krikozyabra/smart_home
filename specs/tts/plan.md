# План реализации модуля tts

Основание — [spec.md](spec.md), [design.md](design.md), `AGENTS.md`.
Целевая платформа — Linux, Raspberry Pi 4b 8/120 (SD-card), CMake 3.20+, C++17.

## 1. Соответствие требованиям спецификации

| Требование | Описание | Реализация | Способ проверки |
|---|---|---|---|
| **TTS-001** | Конфигурация и валидация `TtsConfig` | `TtsConfig.h`, `validateConfig()` | Юнит-тесты валидации путей, частот, дикторов, числа потоков |
| **TTS-002** | Входная строка UTF-8, ударения и пустой ввод | `SileroTtsSynthesizer::synthesize()`, токенизатор | Тесты пустой строки, пробелов, удаления спецсимволов, разметки `+` |
| **TTS-003** | Локальный инференс Silero TTS на CPU | `SileroTtsSynthesizer::Impl`, ONNX Runtime C++ API | Тест пайплайна синтеза на компактной тестовой ONNX-модели |
| **TTS-004** | Формирование `TtsAudio` (`float` и `PCM16`) | `TtsAudio.h`, алгоритм масштабирования отсчётов | Проверка диапазонов значений, частоты дискретизации и длительности |
| **TTS-005** | Прогрев и жизненный цикл ресурсов | Загрузка и прогрев в конструкторе, RAII-деструктор | Серия последовательных вызовов синтеза, проверка move-семантики |
| **TTS-006** | Обработка ошибок и устойчивость | Трансляция ошибок SDK в `runtime_error` / `invalid_argument` | Негативные тесты несуществующих файлов, невалидных конфигураций |

## 2. Пошаговая структура работ (Work Breakdown Structure)

### Шаг 1: Инфраструктура CMake и загрузка моделей
- [ ] 1.1. Создать каталог `tts/` со структурой `include/tts/` и `src/`.
- [ ] 1.2. Создать `tts/CMakeLists.txt` с таргетом `SmartHome::Tts` (C++17, `-Wall -Wextra -Wpedantic`, опция `SMART_HOME_BUILD_TTS`).
- [ ] 1.3. Создать `tts/DownloadModels.cmake` с кастомной целью `tts_models` для опционального скачивания модели Silero TTS (`silero_ru.onnx`) с проверкой SHA256.
- [ ] 1.4. Подключить `tts` в корневом `CMakeLists.txt` и в `cmake/Docs.cmake`.

### Шаг 2: Реализация конфигурации и структур данных
- [ ] 2.1. Создать `tts/include/tts/TtsConfig.h` и `tts/src/TtsConfig.cpp` с реализацией `validateConfig()`.
- [ ] 2.2. Создать `tts/include/tts/TtsAudio.h`.

### Шаг 3: Интерфейс и реализация синтезатора речи
- [ ] 3.1. Создать интерфейс `tts/include/tts/ITtsSynthesizer.h`.
- [ ] 3.2. Создать заголовок `tts/include/tts/SileroTtsSynthesizer.h`.
- [ ] 3.3. Создать `tts/src/SileroTtsSynthesizer.cpp`:
  - Реализовать токенизатор русских символов UTF-8 со встроенным словарем Silero.
  - Подготовить тензоры `input`, `speaker`, `sample_rate`.
  - Реализовать инференс через сессию ONNX Runtime.
  - Реализовать масштабирование нормализованных отсчётов в signed PCM16.
  - Реализовать прогрев в конструкторе и безопасный RAII-деструктор.

### Шаг 4: Модульные и интеграционные тесты
- [ ] 4.1. Создать генератор тестовых фикстур `tests/tts/generate_models.py` (компактная тестовая ONNX-модель синтеза).
- [ ] 4.2. Создать `tests/TtsTests.cpp` с проверками конфигурации, токенизации, пустых строк и жизненного цикла.
- [ ] 4.3. Создать `tests/TtsModelTests.cpp` для опционального бенчмарка реальной модели Silero TTS на Raspberry Pi.
- [ ] 4.4. Зарегистрировать цели `tts_tests` и `tts_model_tests` в `tests/CMakeLists.txt`.

### Шаг 5: Верификация и документация
- [ ] 5.1. Сборка проекта и прохождение тестов `tts_tests` через CTest.
- [ ] 5.2. Проверка регрессии существующих тестов репозитория.
- [ ] 5.3. Сборка документации Doxygen (`cmake --build build --target docs`).
- [ ] 5.4. Фиксация результатов в `specs/tts/walkthrough.md`.

## 3. Сборка и верификация

После завершения реализации проверка выполняется командами:

```sh
cmake -B build -S . -DSMART_HOME_BUILD_TTS=ON
cmake --build build -j 4
ctest --test-dir build -R tts --output-on-failure
cmake --build build --target docs
git diff --check
```

| Команда | Ожидаемый результат |
|---|---|
| `cmake --build build -j 4` | Успешная компиляция без предупреждений компилятора |
| `ctest --test-dir build -R tts --output-on-failure` | Успешное прохождение unit-тестов `tts_tests` |
| `ctest --test-dir build --output-on-failure` | Отсутствие регрессий во всех тестах проекта |
| `cmake --build build --target docs` | Генерация Doxygen-документации API модуля |

Измерения времени синтеза (RTF) и расхода памяти на реальной плате Raspberry Pi 4b производятся отдельно с помощью `tts_model_tests`.
