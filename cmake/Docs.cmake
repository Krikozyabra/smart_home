find_package(Doxygen QUIET)

if(NOT DOXYGEN_FOUND)
    message(WARNING "Doxygen не установлен. Цель сборки документации 'docs' отключена.")
    return()
endif()

set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
set(DOXYGEN_GENERATE_HTML YES)
set(DOXYGEN_GENERATE_MAN NO)
set(DOXYGEN_GENERATE_LATEX NO)
set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
set(DOXYGEN_EXTRACT_ALL YES)
set(DOXYGEN_RECURSIVE YES)
set(DOXYGEN_OUTPUT_LANGUAGE Russian)
set(DOXYGEN_HTML_COLORSTYLE LIGHT)
set(DOXYGEN_GENERATE_TREEVIEW YES)
set(DOXYGEN_SEARCHENGINE YES)
set(DOXYGEN_FILE_PATTERNS "*.h" "*.hpp" "*.hxx" "*.cpp" "*.md")

set(DOC_INPUTS)
foreach(MODULE common devices descriptors drivers registry command execution
               storage device_management discovery logging src tests)
    if(EXISTS "${PROJECT_SOURCE_DIR}/${MODULE}")
        list(APPEND DOC_INPUTS "${PROJECT_SOURCE_DIR}/${MODULE}")
    endif()
endforeach()

if(EXISTS "${PROJECT_SOURCE_DIR}/README.md")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${PROJECT_SOURCE_DIR}/README.md")
    list(APPEND DOC_INPUTS "${PROJECT_SOURCE_DIR}/README.md")
endif()

doxygen_add_docs(docs ${DOC_INPUTS}
    COMMENT "Генерация API-документации с помощью Doxygen"
)
