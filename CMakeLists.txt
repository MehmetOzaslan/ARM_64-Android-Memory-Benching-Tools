cmake_minimum_required(VERSION 3.16)
project(bench_memory C CXX)

# Cross-compile for Android (API 34, arm64-v8a)
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 34)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)

# Android NDK toolchain
set(CMAKE_TOOLCHAIN_FILE "$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")

# Executables
add_executable(bench_memory_aggregated_samples bench_memory_aggregated_samples.cpp)
add_executable(bench_memory_periodic_samples bench_memory_periodic_samples.cpp)

# Common compile/link flags
set(COMMON_CFLAGS  -O3 -fopenmp -fPIE -fdata-sections -ffunction-sections)
set(COMMON_LDFLAGS -pie -Wl,--gc-sections)

# Locate static OpenMP
set(LIBOMP_A "$ENV{ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/lib/clang/17/lib/linux/aarch64/libomp.a")
if(NOT EXISTS "${LIBOMP_A}")
  message(FATAL_ERROR "libomp.a not found at ${LIBOMP_A}")
endif()

# Apply flags + OpenMP to all targets
set(ALL_BINS bench_memory_aggregated_samples bench_memory_periodic_samples)
foreach(tgt IN LISTS ALL_BINS)
  target_compile_options(${tgt} PRIVATE ${COMMON_CFLAGS})
  target_link_options(${tgt}    PRIVATE ${COMMON_LDFLAGS})
  
  target_link_libraries(${tgt} PRIVATE "${LIBOMP_A}")
endforeach()
