cmake_minimum_required(VERSION 3.28)

project(
    Koala1
    VERSION 0.1
    LANGUAGES CXX Fortran
)

set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# 1. Generator — GCC 16 + Fortran
# ============================================================

add_executable(
    KoalaDataGen
    src/generator.cpp
    fortran/fft_batch.f90
)

target_include_directories(
    KoalaDataGen
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(
    KoalaDataGen
    PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:
        -std=c++2c
        -Wall
        -Wextra
        -march=native
        -O3
    >
)

# ============================================================
# 2. Automatyczne generowanie pliku FFT
# ============================================================

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/weights")
set(BIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/weights/fft_32x8x8.bin")

add_custom_command(
    OUTPUT "${BIN_FILE}"
    COMMAND $<TARGET_FILE:KoalaDataGen> "${BIN_FILE}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/generator.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/fortran/fft_batch.f90"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/BoardEncoding.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/FFTBatch.hpp"
    COMMENT "Fortran/GCC: generowanie pliku FFT..."
    VERBATIM
)

# ============================================================
# 3. Clang 17 — kompilacja kerneli GPU dla AMD RX 550
# ============================================================

set(CLANG17_BIN "/usr/lib/llvm-17/bin")
set(CLANG17_CXX "${CLANG17_BIN}/clang++")
set(CLANG17_LIB "/usr/lib/llvm-17/lib")

if(NOT EXISTS "${CLANG17_CXX}")
    message(FATAL_ERROR
        "Clang 17 nie znaleziony: ${CLANG17_CXX}"
    )
endif()

if(NOT EXISTS "${CLANG17_LIB}/libomptarget.so")
    message(FATAL_ERROR
        "Nie znaleziono libomptarget.so w ${CLANG17_LIB}"
    )
endif()

if(NOT EXISTS "${CLANG17_LIB}/libomp.so")
    message(FATAL_ERROR
        "Nie znaleziono libomp.so w ${CLANG17_LIB}"
    )
endif()

message(STATUS "Clang 17: ${CLANG17_CXX}")
message(STATUS "LLVM 17 libraries: ${CLANG17_LIB}")
message(STATUS "GPU architecture: gfx803")

file(
    GLOB GPU_KERNEL_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/kernels/*.cpp"
)

if(NOT GPU_KERNEL_SOURCES)
    message(FATAL_ERROR
        "Nie znaleziono plikow kernels/*.cpp"
    )
endif()

set(GPU_KERNEL_OBJECTS "")

foreach(GPU_SRC IN LISTS GPU_KERNEL_SOURCES)

    get_filename_component(GPU_NAME "${GPU_SRC}" NAME_WE)

    set(
        GPU_OBJ
        "${CMAKE_CURRENT_BINARY_DIR}/${GPU_NAME}.gpu.o"
    )

    add_custom_command(
        OUTPUT "${GPU_OBJ}"

        COMMAND "${CLANG17_CXX}"
            -std=c++17
            -O3
            -fopenmp
            -fopenmp-targets=amdgcn-amd-amdhsa
            -Xopenmp-target=amdgcn-amd-amdhsa
            -march=gfx803
            "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
            -c "${GPU_SRC}"
            -o "${GPU_OBJ}"

        DEPENDS "${GPU_SRC}"

        COMMENT
            "Clang 17: kompilacja GPU ${GPU_NAME} (gfx803)..."

        VERBATIM
    )

    list(APPEND GPU_KERNEL_OBJECTS "${GPU_OBJ}")

endforeach()

add_custom_target(
    GpuKernels
    DEPENDS ${GPU_KERNEL_OBJECTS}
)

# ============================================================
# 4. Host — GCC 16
#    Kod CPU pozostaje kompilowany przez GCC.
# ============================================================

add_library(
    Koala1HostObjects
    OBJECT
        main.cpp
        src/Listener.cpp
        src/Parser.cpp
        src/Response.cpp
)


set_source_files_properties(
    main.cpp
    PROPERTIES
        OBJECT_DEPENDS "${BIN_FILE}"
)

target_include_directories(
    Koala1HostObjects
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_options(
    Koala1HostObjects
    PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:
        -std=c++2c
        -Wall
        -Wextra
        -march=native
        -O3
        -funroll-loops
        -embed-dir=${CMAKE_CURRENT_SOURCE_DIR}
    >
)

# ============================================================
# 5. Końcowe linkowanie — Clang 17 + OpenMP offloading
# ============================================================

set(
    KOALA1_EXE
    "${CMAKE_CURRENT_BINARY_DIR}/Koala1"
)

add_custom_command(
    OUTPUT "${KOALA1_EXE}"

    COMMAND "${CLANG17_CXX}"
        -fopenmp
        -fopenmp-targets=amdgcn-amd-amdhsa
        -Xopenmp-target=amdgcn-amd-amdhsa
        -march=gfx803

        $<TARGET_OBJECTS:Koala1HostObjects>
        ${GPU_KERNEL_OBJECTS}

        "-L${CLANG17_LIB}"
        "-Wl,-rpath,${CLANG17_LIB}"

        -lomptarget
        -lomp

        -o "${KOALA1_EXE}"

    DEPENDS
        Koala1HostObjects
        GpuKernels
        ${GPU_KERNEL_OBJECTS}
        "${BIN_FILE}"

    COMMAND_EXPAND_LISTS
    VERBATIM

    COMMENT
        "Clang 17: koncowe linkowanie Koala1 z OpenMP offloading..."
)

# Nazwa celu jest inna niż nazwa pliku wykonywalnego,
# aby uniknąć cyklicznej zależności w generatorze Make.
add_custom_target(
    Koala1Build
    ALL
    DEPENDS "${KOALA1_EXE}"
)

add_dependencies(
    Koala1Build
    Koala1HostObjects
    GpuKernels
)

# ============================================================
# 6. Informacje o konfiguracji
# ============================================================

message(STATUS "=== Koala1 build configuration ===")
message(STATUS "Host compiler: GCC 16")
message(STATUS "Host OpenMP: bez -fopenmp")
message(STATUS "GPU compiler: Clang 17")
message(STATUS "GPU target: AMDGPU gfx803")
message(STATUS "Final linker driver: Clang 17")
message(STATUS "Offloading wrapper: clang-linker-wrapper")
message(STATUS "OpenMP runtime: LLVM 17")