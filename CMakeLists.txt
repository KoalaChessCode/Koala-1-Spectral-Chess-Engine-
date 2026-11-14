cmake_minimum_required(VERSION 3.28)

# Prefer GCC 16 for host code, then the newest available GCC >= 12, and finally
# Clang >= 15. An explicitly selected CMAKE_CXX_COMPILER or CXX is respected.
if(NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    set(_koala_host_compiler "")

    foreach(_candidate IN ITEMS g++-16 g++-15 g++-14 g++-13 g++-12 g++)
        unset(_koala_candidate_path CACHE)
        find_program(_koala_candidate_path NAMES "${_candidate}")
        if(_koala_candidate_path)
            execute_process(
                COMMAND "${_koala_candidate_path}" -dumpfullversion -dumpversion
                OUTPUT_VARIABLE _koala_candidate_version
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(_koala_candidate_version MATCHES "^([0-9]+)" AND
               CMAKE_MATCH_1 GREATER_EQUAL 12)
                set(_koala_host_compiler "${_koala_candidate_path}")
                break()
            endif()
        endif()
    endforeach()

    if(NOT _koala_host_compiler)
        foreach(_candidate IN ITEMS clang++-20 clang++-19 clang++-18 clang++-17 clang++-16 clang++-15 clang++)
            unset(_koala_candidate_path CACHE)
            find_program(_koala_candidate_path NAMES "${_candidate}")
            if(_koala_candidate_path)
                execute_process(
                    COMMAND "${_koala_candidate_path}" --version
                    OUTPUT_VARIABLE _koala_candidate_version
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                )
                string(REGEX MATCH "version ([0-9]+)\\."
                    _koala_clang_version_match "${_koala_candidate_version}")
                if(CMAKE_MATCH_1 GREATER_EQUAL 15)
                    set(_koala_host_compiler "${_koala_candidate_path}")
                    break()
                endif()
            endif()
        endforeach()
    endif()

    if(_koala_host_compiler)
        set(CMAKE_CXX_COMPILER "${_koala_host_compiler}" CACHE FILEPATH
            "Host C++ compiler selected by Koala1")
    endif()
endif()

project(Koala1 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12)
        message(FATAL_ERROR "Koala1 requires GCC 12 or newer for host code.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
        message(FATAL_ERROR "Koala1 requires Clang 15 or newer for host code.")
    endif()
else()
    message(FATAL_ERROR
        "Unsupported host compiler: ${CMAKE_CXX_COMPILER_ID}. "
        "Use GCC 12+ or Clang 15+."
    )
endif()

set(_koala_compatible_gcc_install_dir "")
foreach(_candidate IN ITEMS g++-13 g++-12)
    unset(_koala_compatible_gxx CACHE)
    find_program(_koala_compatible_gxx NAMES "${_candidate}")
    if(_koala_compatible_gxx)
        execute_process(
            COMMAND "${_koala_compatible_gxx}" -print-file-name=libstdc++.so
            OUTPUT_VARIABLE _koala_compatible_libstdcxx
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(EXISTS "${_koala_compatible_libstdcxx}")
            get_filename_component(_koala_compatible_gcc_install_dir
                "${_koala_compatible_libstdcxx}" DIRECTORY)
            break()
        endif()
    endif()
endforeach()
set(_koala_gcc_toolchain_args "")
if(_koala_compatible_gcc_install_dir)
    list(APPEND _koala_gcc_toolchain_args
        "--gcc-install-dir=${_koala_compatible_gcc_install_dir}")
endif()
set(_koala_host_toolchain_args "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND _koala_gcc_toolchain_args)
    set(_koala_host_toolchain_args "${_koala_gcc_toolchain_args}")
    message(STATUS "Host C++ headers: ${_koala_compatible_gcc_install_dir}")
endif()

message(STATUS "Host compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
    set(KOALA_HOST_CXX_STANDARD 26)
    # CMake 3.28 does not recognize GCC 16's C++26 dialect yet.
    set(KOALA_HOST_CXX_STANDARD_FLAG "-std=c++26")
else()
    set(KOALA_HOST_CXX_STANDARD 23)
    set(KOALA_HOST_CXX_STANDARD_FLAG "")
endif()

set(GPU_ARCH "gfx803" CACHE STRING "AMD GPU architecture for OpenMP offloading")
if(NOT GPU_ARCH MATCHES "^gfx[0-9a-z]+$")
    message(FATAL_ERROR "Invalid GPU_ARCH='${GPU_ARCH}'. Expected an AMD target such as gfx803.")
endif()

set(GPU_CLANGXX "" CACHE FILEPATH "Clang++ compiler with AMDGPU OpenMP offloading support")
if(NOT GPU_CLANGXX)
    find_program(_koala_gpu_clangxx
        NAMES clang++-17 clang++
        HINTS /usr/lib/llvm-17/bin
        DOC "Clang++ compiler with AMDGPU OpenMP offloading support"
    )
    if(_koala_gpu_clangxx)
        set(GPU_CLANGXX "${_koala_gpu_clangxx}" CACHE FILEPATH
            "Clang++ compiler with AMDGPU OpenMP offloading support" FORCE)
    endif()
endif()
if(NOT GPU_CLANGXX OR NOT EXISTS "${GPU_CLANGXX}")
    message(FATAL_ERROR
        "No GPU Clang++ compiler was found. Install Clang 17+ with AMDGPU "
        "OpenMP offloading, or configure with -DGPU_CLANGXX=/path/to/clang++."
    )
endif()

execute_process(
    COMMAND "${GPU_CLANGXX}" --version
    OUTPUT_VARIABLE GPU_CLANG_VERSION_OUTPUT
    RESULT_VARIABLE GPU_CLANG_VERSION_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
string(REGEX MATCH "version ([0-9]+)\\."
    _gpu_clang_version_match "${GPU_CLANG_VERSION_OUTPUT}")
if(NOT GPU_CLANG_VERSION_RESULT EQUAL 0 OR
   NOT CMAKE_MATCH_1 OR CMAKE_MATCH_1 LESS 17)
    message(FATAL_ERROR
        "GPU compiler '${GPU_CLANGXX}' must be Clang 17 or newer. "
        "Detected: ${GPU_CLANG_VERSION_OUTPUT}"
    )
endif()
set(GPU_CLANG_VERSION "${CMAKE_MATCH_1}")

get_filename_component(_gpu_clang_real_path "${GPU_CLANGXX}" REALPATH)
get_filename_component(_gpu_clang_bin_dir "${_gpu_clang_real_path}" DIRECTORY)
get_filename_component(_gpu_llvm_root "${_gpu_clang_bin_dir}" DIRECTORY)

find_library(GPU_OMP_LIBRARY
    NAMES omp
    HINTS "${_gpu_llvm_root}/lib" "${_gpu_llvm_root}/lib64"
          "${_gpu_llvm_root}/lib/x86_64-linux-gnu"
)
find_library(GPU_OMPTARGET_LIBRARY
    NAMES omptarget
    HINTS "${_gpu_llvm_root}/lib" "${_gpu_llvm_root}/lib64"
          "${_gpu_llvm_root}/lib/x86_64-linux-gnu"
)
if(NOT GPU_OMP_LIBRARY OR NOT GPU_OMPTARGET_LIBRARY)
    message(FATAL_ERROR
        "GPU Clang ${GPU_CLANG_VERSION} was found at '${GPU_CLANGXX}', but its "
        "OpenMP runtime libraries (libomp and libomptarget) were not found "
        "under '${_gpu_llvm_root}'. Install the matching LLVM OpenMP runtime."
    )
endif()
get_filename_component(GPU_LLVM_LIB_DIR "${GPU_OMP_LIBRARY}" DIRECTORY)

# Verify the complete OpenMP AMDGPU compile-and-link path during configuration
# so missing offload components fail with an actionable diagnostic.
set(_gpu_offload_probe "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/koala-offload-probe.cpp")
set(_gpu_offload_probe_exe "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/koala-offload-probe")
file(WRITE "${_gpu_offload_probe}"
    "int main() {\n"
    "  int value = 0;\n"
    "  #pragma omp target map(tofrom: value)\n"
    "  { value = 1; }\n"
    "  return value == 1 ? 0 : 1;\n"
    "}\n"
)
execute_process(
    COMMAND "${GPU_CLANGXX}"
        ${_koala_gcc_toolchain_args}
        -std=c++20
        -fopenmp
        -fopenmp-targets=amdgcn-amd-amdhsa
        -Xopenmp-target=amdgcn-amd-amdhsa
        "-march=${GPU_ARCH}"
        "${_gpu_offload_probe}"
        "-L${GPU_LLVM_LIB_DIR}"
        "-Wl,-rpath,${GPU_LLVM_LIB_DIR}"
        -lomptarget
        -lomp
        -o "${_gpu_offload_probe_exe}"
    RESULT_VARIABLE GPU_OFFLOAD_PROBE_RESULT
    OUTPUT_VARIABLE GPU_OFFLOAD_PROBE_OUTPUT
    ERROR_VARIABLE GPU_OFFLOAD_PROBE_ERROR
)
if(NOT GPU_OFFLOAD_PROBE_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Clang '${GPU_CLANGXX}' cannot compile and link an OpenMP offload "
        "program for ${GPU_ARCH}. Install a Clang toolchain with the AMDGPU "
        "OpenMP target, linker wrapper, device libraries, and matching libomp/"
        "libomptarget.\n"
        "${GPU_OFFLOAD_PROBE_OUTPUT}${GPU_OFFLOAD_PROBE_ERROR}"
    )
endif()
message(STATUS "GPU compiler: Clang ${GPU_CLANG_VERSION} (${GPU_CLANGXX})")
if(_koala_compatible_gcc_install_dir)
    message(STATUS "GPU C++ headers: ${_koala_compatible_gcc_install_dir}")
endif()
message(STATUS "GPU architecture: ${GPU_ARCH}")

add_executable(KoalaDataGen src/generator.cpp)
target_include_directories(KoalaDataGen PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(KoalaDataGen PRIVATE cxx_std_23)
set_target_properties(KoalaDataGen PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
target_compile_options(KoalaDataGen PRIVATE
    ${_koala_host_toolchain_args}
    -Wall
    -Wextra
    -march=native
    -O3
    -ffp-contract=off
    ${KOALA_HOST_CXX_STANDARD_FLAG}
)

set(GENERATED_FFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/weights")
file(MAKE_DIRECTORY "${GENERATED_FFT_DIR}")
set(BIN_FILE "${GENERATED_FFT_DIR}/fft_32x8x8.bin")
set(FFT_INCLUDE_FILE "${GENERATED_FFT_DIR}/fft_32x8x8.bin.inc")

add_custom_command(
    OUTPUT "${BIN_FILE}" "${FFT_INCLUDE_FILE}"
    COMMAND $<TARGET_FILE:KoalaDataGen> "${BIN_FILE}" "${FFT_INCLUDE_FILE}"
    DEPENDS
        KoalaDataGen
        "${CMAKE_CURRENT_SOURCE_DIR}/src/generator.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/BoardEncoding.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/FFTBatch.hpp"
    COMMENT "Generating the FFT binary and its portable C++ initializer"
    VERBATIM
)

file(
    GLOB GPU_KERNEL_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/kernels/*.cpp"
)
if(NOT GPU_KERNEL_SOURCES)
    message(FATAL_ERROR "No kernels/*.cpp files were found.")
endif()

set(GPU_KERNEL_OBJECTS "")
foreach(GPU_SRC IN LISTS GPU_KERNEL_SOURCES)
    get_filename_component(GPU_NAME "${GPU_SRC}" NAME_WE)
    set(GPU_OBJ "${CMAKE_CURRENT_BINARY_DIR}/${GPU_NAME}.gpu.o")

    add_custom_command(
        OUTPUT "${GPU_OBJ}"
        COMMAND "${GPU_CLANGXX}"
            ${_koala_gcc_toolchain_args}
            -std=c++20
            -O3
            -fno-math-errno
            -fno-trapping-math
            -fno-signed-zeros
            -ffp-contract=fast
            -fopenmp
            -fopenmp-targets=amdgcn-amd-amdhsa
            -Xopenmp-target=amdgcn-amd-amdhsa
            "-march=${GPU_ARCH}"
            "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
            -c "${GPU_SRC}"
            -o "${GPU_OBJ}"
        DEPENDS "${GPU_SRC}"
        COMMENT "Compiling GPU kernel ${GPU_NAME} for ${GPU_ARCH}"
        VERBATIM
    )
    list(APPEND GPU_KERNEL_OBJECTS "${GPU_OBJ}")
endforeach()

add_custom_target(GpuKernels DEPENDS ${GPU_KERNEL_OBJECTS})

add_library(Koala1HostObjects OBJECT
    main.cpp
    src/Listener.cpp
    src/Parser.cpp
    src/Response.cpp
)
set_source_files_properties(main.cpp PROPERTIES OBJECT_DEPENDS "${FFT_INCLUDE_FILE}")
target_include_directories(Koala1HostObjects PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${GENERATED_FFT_DIR}"
)
target_compile_definitions(Koala1HostObjects PRIVATE
    KOALA_FFT_BINARY_FILE="${BIN_FILE}"
)
target_compile_features(Koala1HostObjects PRIVATE cxx_std_23)
set_target_properties(Koala1HostObjects PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
target_compile_options(Koala1HostObjects PRIVATE
    ${_koala_host_toolchain_args}
    -Wall
    -Wextra
    -march=native
    -O3
    -funroll-loops
    ${KOALA_HOST_CXX_STANDARD_FLAG}
)

set(KOALA1_EXE "${CMAKE_CURRENT_BINARY_DIR}/Koala1")
add_custom_command(
    OUTPUT "${KOALA1_EXE}"
    COMMAND "${GPU_CLANGXX}"
        -fopenmp
        -fopenmp-targets=amdgcn-amd-amdhsa
        -Xopenmp-target=amdgcn-amd-amdhsa
        "-march=${GPU_ARCH}"
        $<TARGET_OBJECTS:Koala1HostObjects>
        ${GPU_KERNEL_OBJECTS}
        "-L${GPU_LLVM_LIB_DIR}"
        "-Wl,-rpath,${GPU_LLVM_LIB_DIR}"
        "${GPU_OMPTARGET_LIBRARY}"
        "${GPU_OMP_LIBRARY}"
        -o "${KOALA1_EXE}"
    DEPENDS
        Koala1HostObjects
        GpuKernels
        ${GPU_KERNEL_OBJECTS}
        "${BIN_FILE}"
        "${FFT_INCLUDE_FILE}"
    COMMAND_EXPAND_LISTS
    VERBATIM
    COMMENT "Linking Koala1 with Clang OpenMP AMDGPU offloading"
)

add_custom_target(Koala1Build ALL DEPENDS "${KOALA1_EXE}")
add_dependencies(Koala1Build Koala1HostObjects GpuKernels)

message(STATUS "Host C++ standard: C++${KOALA_HOST_CXX_STANDARD}")
message(STATUS "Host OpenMP: disabled")
message(STATUS "GPU C++ standard: C++20")
message(STATUS "GPU OpenMP offloading: AMDGPU ${GPU_ARCH}")
