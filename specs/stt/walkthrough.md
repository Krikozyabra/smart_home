# Сборка и проверка STT

## Сборка

По умолчанию STT отключён. `SMART_HOME_BUILD_STT` включает модуль, а
`SMART_HOME_FETCH_SHERPA_ONNX` разрешает загрузку исходников SDK. Проект
собирает `sherpa-onnx` версии 2.0.1 с тем же ONNX Runtime, что и `oww`.

```sh
cmake -S . -B build-stt -DCMAKE_BUILD_TYPE=Release \
  -DSMART_HOME_BUILD_STT=ON -DSMART_HOME_FETCH_SHERPA_ONNX=ON
cmake --build build-stt --target stt_tests
ctest --test-dir build-stt -R '^stt_tests$' --output-on-failure
```

Для готового SDK задайте `SMART_HOME_SHERPA_ONNX_DIR` с заголовком
`include/sherpa-onnx/c-api/c-api.h` и `lib/libsherpa-onnx-c-api.so` из одного
выпуска. `SMART_HOME_ONNXRUNTIME_DIR` должен выбирать заголовки и библиотеку
с тем же ABI, что и ONNX Runtime в SDK. У проверенного архива Linux x86_64
это ONNX Runtime 1.28.2. [Релиз sherpa-onnx 2.0.1](https://github.com/k2-fsa/sherpa-onnx/releases/tag/v2.0.1)

## Модель и API

`cmake --build build-stt --target stt_models` скачивает модель INT8 и словарь
в `build-stt/stt/models/`. Цель проверяет SHA256 обоих файлов и повторно
использует проверенные файлы. Распознавание работает локально после загрузки
модели. [Экспорт модели GigaAM-v3 для sherpa-onnx](https://huggingface.co/csukuangfj/sherpa-onnx-nemo-ctc-giga-am-v3-russian-2025-12-16/tree/32a4c7cc81809bd132e2d935ab99e9e6ab47fbec)

Привяжите потребителя к `SmartHome::Stt`. Передайте готовый монофонический
фрагмент PCM16 при 16 кГц и переиспользуйте распознаватель для следующих
фрагментов.

```cpp
#include <stt/SherpaSttRecognizer.h>

smart_home::stt::SherpaSttRecognizer makeRecognizer() {
    smart_home::stt::SttConfig config;
    config.model_path = "build-stt/stt/models/gigaam_v3_ctc_int8.onnx";
    config.tokens_path = "build-stt/stt/models/tokens.txt";
    return smart_home::stt::SherpaSttRecognizer(config);
}

smart_home::stt::SttResult transcribe(smart_home::stt::ISttRecognizer &stt,
                                     const std::vector<std::int16_t> &pcm16) {
    return stt.recognize(pcm16);
}
```

## Проверки

`stt_tests` работает с компактными локальными ONNX-графами. Он проверяет
нормализацию PCM16, декодирование кириллицы, длительность, распознавание
после удаления загруженных файлов модели и повторное использование сессии
после ошибки инференса.

Для `stt_model_tests` используйте эталонную запись PCM16, моно, 16 кГц. Тест
проверяет повторяемость текста и ожидаемую фразу, если она задана. RTF равен
времени первого распознавания после прогрева, делённому на длительность записи.
[Эталонный WAV](https://huggingface.co/csukuangfj/sherpa-onnx-nemo-ctc-giga-am-v3-russian-2025-12-16/blob/32a4c7cc81809bd132e2d935ab99e9e6ab47fbec/test_wavs/example.wav)

```sh
cmake -S . -B build-stt \
  -DSMART_HOME_STT_TEST_MODEL="$PWD/build-stt/stt/models/gigaam_v3_ctc_int8.onnx" \
  -DSMART_HOME_STT_TEST_TOKENS="$PWD/build-stt/stt/models/tokens.txt" \
  -DSMART_HOME_STT_TEST_WAV=/path/to/example.wav \
  '-DSMART_HOME_STT_TEST_EXPECTED_TEXT=у лукоморья дуб зеленый'
cmake --build build-stt --target stt_model_tests
ctest --test-dir build-stt -R '^stt_model_tests$' -V
cmake --build build-stt --target docs
```

Оба теста STT прошли с готовым и собранным из исходников SDK. CTest прошёл
37 тестов с готовым SDK и 35 при отключённом STT. Тест `stt_tests` прошёл
с AddressSanitizer, LeakSanitizer и UndefinedBehaviorSanitizer.

Сборка Release на Linux x86_64 с двумя потоками распознала 11,29 с речи
примерно за 0,59 с (RTF 0,052). Показатели задержки и памяти на Raspberry
Pi 4b требуют замеров на плате.
