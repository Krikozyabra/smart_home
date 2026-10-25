#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace smart_home::oow {

class OOWException : public std::runtime_error {
  public:
    explicit OOWException(const std::string &msg) : std::runtime_error(msg) {}
};

class MicrophoneNotFoundException : public OOWException {
  public:
    explicit MicrophoneNotFoundException(const std::string &msg)
        : OOWException("Микрофон не найден или не настроен: " + msg) {}
};

class MissingKeywordException : public OOWException {
  public:
    MissingKeywordException(size_t sample_index, float confidence)
        : OOWException("В предоставленной записи микрофона #" +
                       std::to_string(sample_index) +
                       " нет ключевого слова (уверенность: " +
                       std::to_string(confidence) + ")"),
          sample_index_(sample_index), confidence_(confidence) {}

    size_t sampleIndex() const noexcept { return sample_index_; }
    float confidence() const noexcept { return confidence_; }

  private:
    size_t sample_index_;
    float confidence_;
};

class ModelLoadException : public OOWException {
  public:
    explicit ModelLoadException(const std::string &model_path)
        : OOWException("Не удалось загрузить ONNX-модель: " + model_path) {}
};

} // namespace smart_home::oow
