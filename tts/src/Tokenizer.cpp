#include "Tokenizer.h"

#include <fstream>
#include <stdexcept>

namespace smart_home::tts {
namespace {

constexpr std::u32string_view BuiltInAlphabet =
    U"_~абвгдеёжзийклмнопрстуфхцчшщъыьэюя +.,!?…:;–";

[[noreturn]] void invalidUtf8() {
    throw std::invalid_argument("TTS expects valid UTF-8 text and vocabulary.");
}

char32_t takeCodePoint(std::string_view &text) {
    const auto first = static_cast<unsigned char>(text.front());
    text.remove_prefix(1);
    if (first < 0x80)
        return first;
    if (first < 0xc2 || first > 0xf4)
        invalidUtf8();

    const std::size_t remaining = first < 0xe0 ? 1 : first < 0xf0 ? 2 : 3;
    if (text.size() < remaining)
        invalidUtf8();

    char32_t result = first & (0x7f >> (remaining + 1));
    for (std::size_t i = 0; i < remaining; ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if ((byte & 0xc0) != 0x80)
            invalidUtf8();
        result = (result << 6) | (byte & 0x3f);
    }
    constexpr char32_t minimum[] = {0, 0x80, 0x800, 0x10000};
    if (result < minimum[remaining] || result > 0x10ffff ||
        (result >= 0xd800 && result <= 0xdfff))
        invalidUtf8();
    text.remove_prefix(remaining);
    return result;
}

char32_t lowerCase(char32_t symbol) {
    if (symbol >= U'А' && symbol <= U'Я')
        return symbol + (U'а' - U'А');
    if (symbol == U'Ё')
        return U'ё';
    if (symbol >= U'A' && symbol <= U'Z')
        return symbol + (U'a' - U'A');
    return symbol;
}

bool isWhitespace(char32_t symbol) {
    // Unicode White_Space. Keep tokenization independent of the process locale.
    return (symbol >= U'\t' && symbol <= U'\r') || symbol == U' ' ||
           symbol == 0x85 || symbol == 0xa0 || symbol == 0x1680 ||
           (symbol >= 0x2000 && symbol <= 0x200a) || symbol == 0x2028 ||
           symbol == 0x2029 || symbol == 0x202f || symbol == 0x205f ||
           symbol == 0x3000;
}

bool isRussianVowel(char32_t symbol) {
    constexpr std::u32string_view vowels = U"аеёиоуыэюя";
    return vowels.find(symbol) != std::u32string_view::npos;
}

} // namespace

Tokenizer::Tokenizer(const std::string &path) {
    if (path.empty()) {
        for (const auto symbol : BuiltInAlphabet)
            ids_.emplace(symbol, ids_.size());
        return;
    }

    std::ifstream file(path, std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::string_view text(line);
        if (ids_.empty() && text.substr(0, 3) == "\xef\xbb\xbf")
            text.remove_prefix(3);
        if (text.empty())
            throw std::invalid_argument(
                "TTS vocabulary requires one symbol per line.");
        const auto symbol = takeCodePoint(text);
        if (!text.empty() || symbol == U'\0')
            throw std::invalid_argument(
                "TTS vocabulary requires one symbol per line.");
        if (!ids_.emplace(symbol, ids_.size()).second)
            throw std::invalid_argument(
                "TTS vocabulary contains a duplicate symbol.");
    }
    if (!file.eof() || file.bad())
        throw std::invalid_argument("Could not read TTS vocabulary: " + path);

    for (const auto symbol : std::u32string_view(U"~ .+"))
        if (ids_.count(symbol) == 0)
            throw std::invalid_argument(
                "TTS vocabulary requires '~', space, '.' and '+'.");
}

void Tokenizer::encode(std::string_view text,
                       std::vector<std::int64_t> &tokens) const {
    tokens.clear();
    char32_t last = U'\0';
    bool pending_space = false;
    while (!text.empty()) {
        const auto symbol = lowerCase(takeCodePoint(text));
        if (isWhitespace(symbol)) {
            pending_space = !tokens.empty();
            continue;
        }
        if (symbol == U'_' || symbol == U'~')
            continue;
        if (symbol == U'+') {
            auto next = text;
            if (next.empty())
                continue;
            const auto vowel = lowerCase(takeCodePoint(next));
            if (!isRussianVowel(vowel) || ids_.count(vowel) == 0)
                continue;
        }
        const auto token = ids_.find(symbol);
        if (token == ids_.end())
            continue;
        if (pending_space)
            tokens.push_back(ids_.at(U' '));
        tokens.push_back(token->second);
        pending_space = false;
        last = symbol;
    }
    if (tokens.empty())
        return;
    if (last != U'.' && last != U'!' && last != U'?')
        tokens.push_back(ids_.at(U'.'));
    tokens.push_back(ids_.at(U'~'));
}

} // namespace smart_home::tts
