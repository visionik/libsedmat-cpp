cmake_minimum_required(VERSION 3.20)
project(libsedmat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sedmat STATIC src/sedmat.cpp src/intellisense.cpp)
target_include_directories(sedmat PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
if(MSVC)
    target_compile_options(sedmat PRIVATE /W4)
else()
    target_compile_options(sedmat PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(conformance_test tests/conformance_test.cpp)
target_link_libraries(conformance_test PRIVATE sedmat)
add_test(NAME conformance_test COMMAND conformance_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Examples
add_executable(sedmat_lsp_server examples/lsp_server.cpp)
target_link_libraries(sedmat_lsp_server PRIVATE sedmat)

if(NOT WIN32)
    add_executable(sedmat_doc_playground examples/doc_playground.cpp)
    target_link_libraries(sedmat_doc_playground PRIVATE sedmat)

    add_executable(sedmat_slide_deck examples/slide_deck.cpp)
    target_link_libraries(sedmat_slide_deck PRIVATE sedmat)
endif()
