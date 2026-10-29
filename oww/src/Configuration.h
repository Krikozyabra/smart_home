#pragma once

#include "oww/OwwConfig.h"

namespace smart_home::oww::detail {
// Resolves companion paths and validates before creating any inference session.
OwwConfig validatedConfig(OwwConfig config);
} // namespace smart_home::oww::detail
