#include <stdexcept>
#include <string>

inline void require(bool condition, const std::string message) {
    if (!condition)
        throw std::runtime_error(message);
}

template <typename Exception, typename Function>
void expectException(Function function) {
    try {
        function();
    } catch (const Exception &) {
        return;
    }

    throw std::runtime_error("Expected exception was not thrown");
}
