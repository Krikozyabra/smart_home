# smart_home

# Building
prebuild
`cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE`
build
`cmake --build build`

# Test
`ctest --build-dir build --output-on-failure`

# Tasks
- connect spdlog
- create DiscoveryReport
- isolating errros
- logging
