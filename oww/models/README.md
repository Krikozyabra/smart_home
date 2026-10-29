В этот каталог команда `cmake -P oww/DownloadModels.cmake` из корня проекта
загружает комплект для слова «Нафаня»:

| Файл | Источник |
| --- | --- |
| `nafanya.onnx` | [Модель slavonnet, добавленная в коллекцию Home Assistant](https://github.com/fwartner/home-assistant-wakewords-collection/pull/72), commit `3342878221ae06b09f46cc38a74182ac00b926df` |
| `melspectrogram.onnx` | [Релиз OpenWakeWord v0.5.1](https://github.com/dscripka/openWakeWord/releases/tag/v0.5.1) |
| `embedding_model.onnx` | [Релиз OpenWakeWord v0.5.1](https://github.com/dscripka/openWakeWord/releases/tag/v0.5.1) |

Скачанные бинарные файлы исключены из Git. Повторный запуск сохраняет уже
существующие непустые файлы. Загрузка выполняется только отдельной командой,
не при сборке или обработке аудио. Можно выбрать другой каталог:
`cmake -DOWW_MODELS_DIR=/path/to/models -P oww/DownloadModels.cmake`.
Незавершённые загрузки не устанавливаются под именем модели.

Это публичная модель сообщества; сведения об обучении и качестве приведены
в [описании автора](https://github.com/fwartner/home-assistant-wakewords-collection/blob/main/ru/nafanya/readme.md).
Качество распознавания на вашем микрофоне проверяется отдельно аудиозаписями.
Успешный smoke-тест подтверждает загрузку и вычисления, но не полноту распознавания.

Исходный код коллекции и условия распространения опубликованы в её
[LICENSE](https://github.com/fwartner/home-assistant-wakewords-collection/blob/main/LICENSE);
сведения о лицензировании OpenWakeWord доступны в
[upstream README](https://github.com/dscripka/openWakeWord#license).
