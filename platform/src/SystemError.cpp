#include "platform/SystemError.h"

#include <string>
#include <system_error>

namespace smart_home::platform {

void throwSystemError(std::string_view operation, int error) {
    throw std::system_error(error, std::generic_category(),
                            std::string(operation));
}

} // namespace smart_home::platform
