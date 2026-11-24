#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smart_home::tts {

class Tokenizer {
  public:
    explicit Tokenizer(const std::string &path);
    void encode(std::string_view text, std::vector<std::int64_t> &tokens) const;

  private:
    std::unordered_map<char32_t, std::int64_t> ids_;
};

} // namespace smart_home::tts
