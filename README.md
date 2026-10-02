# smart_home

# Building
prebuild
`cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE`
build
`cmake --build build`

# Test
`ctest --build-dir build --output-on-failure`

# Документация

API-документация на русском языке генерируется Doxygen со стандартной встроенной темой.
Необходимы CMake и Doxygen; Graphviz используется для диаграмм, если установлен.

```sh
cmake -B build -S . -DBUILD_DOCS=ON
cmake --build build --target docs
```

Откройте `build/docs/html/index.html` в браузере. Доступны поиск и иерархия классов.
Чтобы отключить настройку документации, используйте `-DBUILD_DOCS=OFF`.

# Tasks
- connect spdlog
- create DiscoveryReport
- isolating errros
- logging
