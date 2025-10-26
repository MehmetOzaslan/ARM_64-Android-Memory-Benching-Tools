#ifndef MEMORY_SETTINGS_H
#define MEMORY_SETTINGS_H

#include <cstdint>

// ============================================================================
// Build Configuration Flags
// ============================================================================

// Uncomment to enable features:
// #define USE_NONTEMPORAL
// #define PREFETCH_NEXT
// #define RANDOMIZE_ALLOC

// ============================================================================
// Thread Configuration
// ============================================================================

#ifndef NUM_CORES
#define NUM_CORES 8
#endif

#ifndef THREADS
#define THREADS 8
#endif

// ============================================================================
// Block and Memory Configuration
// ============================================================================

#define BLOCK_SIZE 512  // How much is copied by the inline assembly functions (see memory_instructions.h)

// ============================================================================
// Prefetch Configuration
// ============================================================================

#ifndef PF_DIST_BLOCKS
#define PF_DIST_BLOCKS 2  // prefetch this many 512B blocks ahead
#endif

#ifndef PF_EVERY
#define PF_EVERY 1  // prefetch every N iterations (1 = every iter)
#endif

#ifndef PF_BYTES
#define PF_BYTES 512  // total bytes to prefetch per iteration
#endif

// ============================================================================
// String Conversion Macros
// ============================================================================

#define STR1(x) #x
#define STR(x)  STR1(x)

// ============================================================================
// Feature Flag String Conversions
// ============================================================================

#ifdef USE_NONTEMPORAL
  #define USE_NT_STR "1"
#else
  #define USE_NT_STR "0"
#endif

#ifdef PREFETCH_NEXT
  #define PF_STR "1"
#else
  #define PF_STR "0"
#endif

#ifdef RANDOMIZE_ALLOC
  #define RAND_STR "1"
#else
  #define RAND_STR "0"
#endif

// ============================================================================
// Operation Selection Macros
// ============================================================================

#include "memory_instructions.h"

#ifdef USE_NONTEMPORAL
  #define READ_512_BYTES(addr)         READ_NONTEMPORAL_512_BYTES((addr))
  #define WRITE_512_BYTES(dst)         WRITE_NONTEMPORAL_512_BYTES((dst))
  #define COPY_512_BYTES(dst, src)     COPY_NONTEMPORAL_512_BYTES((dst), (src))
#else
  #define READ_512_BYTES(addr)         READ_CACHED_512_BYTES((addr))
  #define WRITE_512_BYTES(dst)         WRITE_CACHED_512_BYTES((dst))
  #define COPY_512_BYTES(dst, src)     COPY_CACHED_512_BYTES((dst), (src))
#endif

// ============================================================================
// Runtime Configuration
// ============================================================================

#ifndef TOTAL_RUNTIME_SEC
#define TOTAL_RUNTIME_SEC 3.0  // Run for n seconds per operation
#endif

#ifndef TIME_PERIOD_SEC
#define TIME_PERIOD_SEC 0.0001  // Measurement period (deprecated, may cause branch prediction issues)
#endif

// ============================================================================
// Chunk Configuration
// ============================================================================

#ifndef BLOCKS_PER_CHUNK
#define BLOCKS_PER_CHUNK 1024  // Process this many consecutive blocks of 512 bytes per chunk
#endif

// ============================================================================
// Buffer Configuration
// ============================================================================

#ifndef BUFFER_SIZE
#define BUFFER_SIZE (1024ULL * 1024ULL * 1024ULL / 2)  // 512 MiB per buffer
#endif

#define MAX_BLOCKS (BUFFER_SIZE / BLOCK_SIZE)

#if ((BUFFER_SIZE % BLOCK_SIZE) != 0)
# error "BUFFER_SIZE must be a multiple of BLOCK_SIZE"
#endif

// ============================================================================
// Benchmark-Specific Configuration
// ============================================================================

// For aggregate_stream.cpp
#ifndef N_TIMES
#define N_TIMES 50
#endif

#ifndef BYTES
#define BYTES (1ULL << 30 >> 4)  // 0.25 GiB
#endif

#if ((BYTES % BLOCK_SIZE) != 0)
# error "BYTES must be a multiple of BLOCK_SIZE"
#endif

// Core group bitmasks (bit i set means core i belongs to set)
// Core 7: high, cores 4-6: medium, cores 0-3: low
static std::uint8_t LOW_CORES_MASK    = 0b00001111; // Cores 0-3
static std::uint8_t MEDIUM_CORES_MASK = 0b01110000; // Cores 4-6
static std::uint8_t HIGH_CORES_MASK   = 0b10000000; // Core 7
static std::uint8_t USER_CORE_MASK    = 0b11000001;

// ============================================================================
// Configuration String Macros
// ============================================================================

// For instantaneous_stream.cpp
#define CONFIG_STR \
  "use_nt=" USE_NT_STR \
  ";pf=" PF_STR \
  ";rand=" RAND_STR \
  ";threads=" STR(THREADS) \
  ";pf_dist=" STR(PF_DIST_BLOCKS) \
  ";pf_every=" STR(PF_EVERY) \
  ";pf_bytes=" STR(PF_BYTES) \
  ";time_period=" STR(TIME_PERIOD_SEC) \
  ";total_runtime=" STR(TOTAL_RUNTIME_SEC) \
  ";blocks_per_chunk=" STR(BLOCKS_PER_CHUNK) \
  ";buffer_size=" STR(BUFFER_SIZE)

// For aggregate_stream.cpp
#define CONFIG_TAG \
  "," \
  "use_nt=" USE_NT_STR \
  ";pf=" PF_STR \
  ";rand=" RAND_STR \
  ";threads=" STR(THREADS) \
  ";pf_dist=" STR(PF_DIST_BLOCKS) \
  ";pf_every=" STR(PF_EVERY) \
  ";pf_bytes=" STR(PF_BYTES) \
  ";blk=" STR(BLOCK_SIZE) \
  ";blk_per_chunk=" STR(BLOCKS_PER_CHUNK) \
  ";bytes=" STR(BYTES)

// ============================================================================
// Helper Functions
// ============================================================================

#include <sched.h>
#include <random>
#include <cstring>

// Set process-wide affinity mask to allow specific cores
static inline bool set_process_affinity_mask(std::uint8_t core_mask) {
  cpu_set_t set;
  CPU_ZERO(&set);
  for (int i = 0; i < 8; ++i) {
    if (core_mask & (1 << i)) {
      CPU_SET(i, &set);
    }
  }
  return sched_setaffinity(0, sizeof(set), &set) == 0;
}

// Pin current thread to a specific CPU
static inline bool pin_to_cpu(int cpu) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set) == 0;
}

// Get current process affinity mask
static inline std::uint8_t get_process_affinity_mask() {
  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(0, sizeof(set), &set) != 0) {
    return 0;
  }
  std::uint8_t mask = 0;
  for (int i = 0; i < 8; ++i) {
    if (CPU_ISSET(i, &set)) {
      mask |= (1 << i);
    }
  }
  return mask;
}

// ============================================================================
// Core Mask Utilities
// ============================================================================

// Count number of set bits in the mask
static inline int count_cores_in_mask(std::uint8_t mask) {
  int count = 0;
  for (int i = 0; i < 8; ++i) {
    if (mask & (1 << i)) {
      ++count;
    }
  }
  return count;
}

// Get the core ID for the Nth thread (0-indexed)
// Returns the position of the Nth set bit in the mask
static inline int get_core_for_thread(std::uint8_t mask, int thread_id) {
  int count = 0;
  for (int i = 0; i < 8; ++i) {
    if (mask & (1 << i)) {
      if (count == thread_id) {
        return i;
      }
      ++count;
    }
  }
  return -1;
}

// Fast xorshift64* random number generator
static inline std::uint64_t xorshift64star(std::uint64_t* state) {
  std::uint64_t x = *state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  *state = x;
  return x * 0x2545F4914F6CDD1DULL;
}

static inline void fill_random(void* dst, std::size_t nbytes, std::uint64_t seed = 0x12345678ULL) {
  auto* p64   = static_cast<std::uint64_t*>(dst);
  auto* p8    = static_cast<std::uint8_t*>(dst);
  std::size_t words = nbytes / 8;
  std::size_t tail  = nbytes & 7;

  // Use OpenMP for parallelization with thread-local state
  #pragma omp parallel
  {
    std::uint64_t thread_seed = seed + omp_get_thread_num();
    #pragma omp for
    for (std::size_t i = 0; i < words; ++i) {
      p64[i] = xorshift64star(&thread_seed);
    }
  }
  
  // Handle remaining bytes
  if (tail) {
    std::uint64_t last_seed = seed + words;
    std::uint64_t last = xorshift64star(&last_seed);
    for (std::size_t t = 0; t < tail; ++t) {
      p8[words * 8 + t] = static_cast<std::uint8_t>(last >> (t * 8));
    }
  }
}

static inline void fill_zeros(void* dst, std::size_t nbytes) {
  std::memset(dst, 0, nbytes);
}

#endif // MEMORY_SETTINGS_H


