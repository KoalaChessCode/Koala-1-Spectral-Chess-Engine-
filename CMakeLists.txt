cmake_minimum_required(VERSION 3.28)

project(
    Koala1
    VERSION 0.1
    LANGUAGES CXX Fortran
)

set(CMAKE_CXX_EXTENSIONS OFF)


add_executable(
    KoalaDataGen
    src/generator.cpp
    fortran/fft_batch.f90
)

target_include_directories(
    KoalaDataGen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)


set(BIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/fft_32x8x8.bin")


add_custom_command(
    OUTPUT ${BIN_FILE}
    COMMAND KoalaDataGen "${BIN_FILE}"
    DEPENDS KoalaDataGen
    COMMENT "Automatyczne obliczenia: Fortran generuje plik .bin..."
)


add_executable(
    Koala1
    main.cpp
    src/Listener.cpp
    src/Parser.cpp
    src/Response.cpp
    ${BIN_FILE}
)

target_include_directories(
    Koala1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(
    Koala1
    PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:
        -std=c++2c
        -Wall 
        -Wextra
        -march=native
        -embed-dir=${CMAKE_CURRENT_SOURCE_DIR}
    >
)


