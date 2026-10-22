add_library(smart_home_ontology STATIC
    src/LadybugOntologyStorage.cpp
    src/SnapshotFiles.cpp
    src/OntologyConfig.cpp)
add_library(SmartHome::Ontology ALIAS smart_home_ontology)
target_include_directories(smart_home_ontology PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(smart_home_ontology PUBLIC SmartHome::Common
    PRIVATE Ladybug::SDK SmartHome::Logging Threads::Threads)
target_compile_features(smart_home_ontology PUBLIC cxx_std_17)
set_target_properties(smart_home_ontology PROPERTIES
    CXX_STANDARD ${SMART_HOME_LADYBUG_CXX_STANDARD} CXX_STANDARD_REQUIRED ON)
target_compile_options(smart_home_ontology PRIVATE -Wall -Wextra -Wpedantic)
