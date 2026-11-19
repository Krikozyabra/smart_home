#pragma once

#if defined(SMART_HOME_RUNTIME_TESTING) && SMART_HOME_RUNTIME_TESTING == 1
#include "persistence/PersistenceTypes.h"

namespace smart_home::persistence::testing {
void testPoint(const char *point, Revision revision);
}

#define PERSISTENCE_TEST_POINT(point, revision)                                \
    ::smart_home::persistence::testing::testPoint(point, revision)
#else
#define PERSISTENCE_TEST_POINT(point, revision) ((void)0)
#endif
