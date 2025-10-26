# ARM Memory Benchmarks

Memory performance benchmarks for ARM64 Android devices, designed to measure read, write, and copy operations.

## Overview

This project contains two memory benchmark tools:
- **bench_memory_aggregated_samples**: Aggregates samples over time periods
- **bench_memory_periodic_samples**: Performs periodic sampling measurements.

Both tools use OpenMP for parallel execution and are optimized for ARM64 architecture.

## Requirements

- Android NDK (set `ANDROID_NDK_HOME` environment variable)
- CMake 3.16 or higher
- Android device with ADB access
- ARM64-v8a architecture

## Build and Run

### Quick Start

Two simple scripts are provided to build, push, and run the benchmarks:

```bash
# Run aggregated samples benchmark
./run_aggregated.sh

# Run periodic samples benchmark
./run_periodic.sh
```

### Manual Build

```bash
# Configure build
cmake -B build -S .

# Build specific target
cmake --build build --target bench_memory_aggregated_samples -j
cmake --build build --target bench_memory_periodic_samples -j

# Push to device
adb push build/bench_memory_aggregated_samples /data/local/tmp/
adb shell chmod +x /data/local/tmp/bench_memory_aggregated_samples

# Run
adb shell /data/local/tmp/bench_memory_aggregated_samples
```

## Configuration

The benchmarks use static OpenMP linking to avoid runtime library dependencies on the Android device. Key compilation flags:

## Output

The benchmarks output CSV data to stdout with the following fields:
- `core_mask`: CPU affinity mask
- `op`: Operation type (write/read/copy)
- `tid`: Thread ID
- `total_iterations`: Number of iterations completed
- `start_time`, `end_time`: Timestamps
- `blocks_per_chunk`: Chunk size configuration
- `chunk_idx`: Current chunk index

## Project Structure

```
.
├── CMakeLists.txt                    # Build configuration
├── bench_memory_aggregated_samples.cpp    # Aggregated benchmark
├── bench_memory_periodic_samples.cpp      # Periodic benchmark
├── memory_settings.h                 # Memory operation implementations
├── memory_instructions.h             # Instruction definitions
├── run_aggregated.sh                 # Quick run script for aggregated
├── run_periodic.sh                   # Quick run script for periodic
└── README.md                         # This file
```

## Notes

- Binaries are statically linked with OpenMP.
- Default configuration uses 512-byte blocks of nontemporal or cached vector instructions. Note that nontemporal instructions are just a hardware hint and may not always be implemented. Just as a heads up there's also some other libraries that have these implemented.
- The way core masks are set up currently only allows 8 cores. Note that this was built with the Snapdragon 865 in mind.
- Write performance is suspiciously high, may be due to either caching effects / hw optimizations or writes not being properly committed. May need to write some tests to verify.
