#pragma once

#include "descriptors/DescriptorTypes.h"
#include "devices/types/Value.h"

#include <string_view>
#include <vector>

namespace smart_home::cli {

/// @brief Проверенная операция из каталога возможностей.
struct DeviceOperation {
    std::string_view
        id; ///< Идентификатор из каталога со статическим сроком жизни.
    OperationType type; ///< Чтение или запись по дескриптору операции.
    std::vector<Value>
        arguments; ///< Пустой список при чтении, одно значение при записи.
};

} // namespace smart_home::cli
