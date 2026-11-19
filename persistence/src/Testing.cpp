#include "Testing.h"
#include "FileUtils.h"

#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>

namespace fs = std::filesystem;

namespace smart_home::persistence::testing {

void testPoint(const char *point, Revision revision) {
    const char *base = std::getenv("SMART_HOME_RUNTIME_TEST_CONTROL");
    if (!base)
        return;
    const fs::path control(base);
    const auto key = std::string(point) + "-" + std::to_string(revision);
    {
        detail::FileDescriptor marker(detail::openPath(
            control / (key + ".reached"), O_WRONLY | O_CREAT | O_TRUNC));
        detail::writeBytes(marker.get(), "ready\n");
        marker.close();
    }
    const auto gate = control / (key + ".gate");
    if (fs::exists(gate)) {
        const detail::FileDescriptor fifo(detail::openPath(gate, O_RDONLY));
        char byte;
        detail::readBytes(fifo.get(), &byte, 1);
    }
    if (fs::exists(control / (key + ".fail")))
        throw std::runtime_error("Injected runtime persistence failure.");
}

} // namespace smart_home::persistence::testing
