#include <atomic>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <omp.h>

#include "memory_settings.h"

using u64 = std::uint64_t;

// ============================================================================
// Configuration: Compile-time constants for maximum optimization
// ============================================================================

// Operation types enum
enum class Operation {
  WRITE = 0,
  READ = 1,
  COPY = 2
};

// Convert operation enum to string
static inline const char* operation_to_string(Operation op) {
  switch (op) {
    case Operation::WRITE: return "write";
    case Operation::READ:  return "read";
    case Operation::COPY:  return "copy";
    default: return "unknown";
  }
}

// ============================================================================
// Measurement Result Structure
// ============================================================================

struct MeasurementResult {
  Operation op;                    // Operation type
  int tid;                         // Thread ID
  std::uint64_t total_iterations;  // Total iterations completed
  double start_time;               // Start time in seconds
  double end_time;                 // End time in seconds
  std::size_t blocks_per_chunk;    // Blocks per chunk
  std::size_t chunk_idx;           // Chunk index where it last left off
};

// Chunk configuration: how many blocks each thread processes contiguously
// Default value, can be overridden by command-line argument
#ifndef BLOCKS_PER_CHUNK_DEFAULT
#define BLOCKS_PER_CHUNK_DEFAULT 1024  // Process this many consecutive blocks of 512 per chunk.
#endif

// Static aligned buffers
alignas(4096) static std::uint8_t read_buf_global [BUFFER_SIZE];
alignas(4096) static std::uint8_t write_buf_global[BUFFER_SIZE];

#define USE_STRIDED_BARRIER





// ============================================================================
// Time-based Benchmark Functions
// ============================================================================

// Write benchmark: process one chunk of blocks
// Returns number of blocks processed, sets actual start/end ticks
FORCE_INLINE std::uint64_t
chunk_write_benchmark(std::uint8_t*   w, const std::uint64_t max_blocks, 
                      std::uint64_t& start_tick, std::uint64_t& end_tick,
                      std::size_t& chunk_start_idx, const std::size_t blocks_per_chunk) {
  start_tick = get_timestamp();
  std::uint64_t iterations = 0;  // Total blocks processed in this chunk

  // Process one chunk of consecutive blocks
  const std::size_t chunk_end = std::min(chunk_start_idx + blocks_per_chunk, (std::size_t)max_blocks);
  
  for (std::size_t block_idx = chunk_start_idx; block_idx < chunk_end; ++block_idx) {
    #ifdef PREFETCH_NEXT
      if (PF_EVERY == 1 || ((block_idx - chunk_start_idx) % PF_EVERY) == 0) {
        const std::size_t pf_idx = block_idx + PF_DIST_BLOCKS;
        if (pf_idx < max_blocks) {
          PREFETCH_N_BYTES_WRITE_HINT(w + pf_idx * BLOCK_SIZE, PF_BYTES);
        }
      }
    #endif

    WRITE_512_BYTES(w + block_idx * BLOCK_SIZE);
    ++iterations;
  }
  
  // Move to next chunk, wrapping around if needed
  chunk_start_idx += blocks_per_chunk;
  if (chunk_start_idx >= max_blocks) chunk_start_idx = 0;

  end_tick = get_timestamp();
  return iterations;
}

// Read benchmark: process one chunk of blocks
// Returns number of blocks processed, sets actual start/end ticks
FORCE_INLINE std::uint64_t
chunk_read_benchmark(const std::uint8_t*   r, const std::uint64_t max_blocks, 
                     std::uint64_t& start_tick, std::uint64_t& end_tick,
                     std::size_t& chunk_start_idx, const std::size_t blocks_per_chunk) {
  start_tick = get_timestamp();
  std::uint64_t iterations = 0;  // Total blocks processed in this chunk

  // Process one chunk of consecutive blocks
  const std::size_t chunk_end = std::min(chunk_start_idx + blocks_per_chunk, (std::size_t)max_blocks);
  
  for (std::size_t block_idx = chunk_start_idx; block_idx < chunk_end; ++block_idx) {
    #ifdef PREFETCH_NEXT
      if (PF_EVERY == 1 || ((block_idx - chunk_start_idx) % PF_EVERY) == 0) {
        const std::size_t pf_idx = block_idx + PF_DIST_BLOCKS;
        if (pf_idx < max_blocks) {
          PREFETCH_N_BYTES_READ_HINT(r + pf_idx * BLOCK_SIZE, PF_BYTES);
        }
      }
    #endif

    READ_512_BYTES(r + block_idx * BLOCK_SIZE);
    ++iterations;
  }
  
  // Move to next chunk, wrapping around if needed
  chunk_start_idx += blocks_per_chunk;
  if (chunk_start_idx >= max_blocks) chunk_start_idx = 0;

  end_tick = get_timestamp();
  return iterations;
}

// Copy benchmark: process one chunk of blocks
// Returns number of blocks processed, sets actual start/end ticks
FORCE_INLINE std::uint64_t
chunk_copy_benchmark(std::uint8_t*   w, const std::uint8_t*   r, 
                     const std::uint64_t max_blocks, 
                     std::uint64_t& start_tick, std::uint64_t& end_tick,
                     std::size_t& chunk_start_idx, const std::size_t blocks_per_chunk) {
  start_tick = get_timestamp();
  std::uint64_t iterations = 0;  // Total blocks processed in this chunk

  // Process one chunk of consecutive blocks
  const std::size_t chunk_end = std::min(chunk_start_idx + blocks_per_chunk, (std::size_t)max_blocks);
  
  for (std::size_t block_idx = chunk_start_idx; block_idx < chunk_end; ++block_idx) {
    #ifdef PREFETCH_NEXT
      if (PF_EVERY == 1 || ((block_idx - chunk_start_idx) % PF_EVERY) == 0) {
        const std::size_t pf_idx = block_idx + PF_DIST_BLOCKS;
        if (pf_idx < max_blocks) {
          PREFETCH_N_BYTES_READ_HINT(r + pf_idx * BLOCK_SIZE, PF_BYTES);
        }
      }
    #endif

    COPY_512_BYTES(w + block_idx * BLOCK_SIZE, r + block_idx * BLOCK_SIZE);
    ++iterations;
  }
  
  // Move to next chunk, wrapping around if needed
  chunk_start_idx += blocks_per_chunk;
  if (chunk_start_idx >= max_blocks) chunk_start_idx = 0;

  end_tick = get_timestamp();
  return iterations;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
  // Parse command-line arguments
  std::size_t blocks_per_chunk = BLOCKS_PER_CHUNK_DEFAULT;

  for (int i = 1; i < argc; ++i) {

    if (strcmp(argv[i], "--cores") == 0 || strcmp(argv[i], "-c") == 0) {
      if (i + 1 < argc) {
        char* endptr;
        unsigned long mask_value = strtoull(argv[++i], &endptr, 0); // 0 allows both decimal and hex
        if (*endptr != '\0') {
          std::cerr << "Error: Invalid core mask format '" << argv[i-1] << "'" << std::endl;
          return 1;
        }
        if (mask_value > 255) {
          std::cerr << "Error: Core mask must be <= 255 (0xFF)" << std::endl;
          return 1;
        }
        USER_CORE_MASK = (std::uint8_t)mask_value;
      } else {
        std::cerr << "Error: --cores requires an argument" << std::endl;
        return 1;
      }
    } else if (strcmp(argv[i], "--stride") == 0 || strcmp(argv[i], "-s") == 0) {
      if (i + 1 < argc) {
        blocks_per_chunk = (std::size_t)strtoull(argv[++i], nullptr, 10);
        if (blocks_per_chunk == 0) {
          std::cerr << "Error: stride must be > 0" << std::endl;
          return 1;
        }
      } else {
        std::cerr << "Error: --stride requires an argument" << std::endl;
        return 1;
      }
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      std::cout << "Usage: " << argv[0] << " [OPTIONS]\n";
      std::cout << "Options:\n";
      std::cout << "  --cores, -c <mask>    CPU core mask (decimal or hex)\n";
      std::cout << "                        Default: " << (int)USER_CORE_MASK << "\n";
      std::cout << "  --stride, -s <size>   Blocks per chunk (stride size)\n";
      std::cout << "                        Default: " << BLOCKS_PER_CHUNK_DEFAULT << "\n";
      std::cout << "  --help, -h            Show this help message\n";
      return 0;
    } else {
      std::cerr << "Error: Unknown argument '" << argv[i] << "'" << std::endl;
      std::cerr << "Use --help for usage information" << std::endl;
      return 1;
    }
  }
  // Keep stderr unbuffered for progress messages
  setvbuf(stderr, nullptr, _IONBF, 0);
  std::cerr.setf(std::ios::unitbuf);

  // Check current process affinity mask
  std::uint8_t current_affinity = get_process_affinity_mask();
  std::cerr << "Current process affinity mask: 0b" << std::bitset<8>(current_affinity) 
            << " (0x" << std::hex << (int)current_affinity << std::dec << ")" << std::endl;

  // Determine number of threads from USER_CORE_MASK
  const int num_threads = count_cores_in_mask(USER_CORE_MASK);
  
  std::cerr << "========================================" << std::endl;
  std::cerr.flush();
  std::cerr << "Instantaneous Stream Benchmark" << std::endl;
  std::cerr << "========================================" << std::endl;
  std::cerr << "Configuration:" << std::endl;
  std::cerr << "  Current Core Mask: 0b" << std::bitset<8>(current_affinity) << " (0x" << std::hex << (int)current_affinity << std::dec << ")" << std::endl;
  std::cerr << "  User Core Mask:   0b" << std::bitset<8>(USER_CORE_MASK) << " (0x" << std::hex << (int)USER_CORE_MASK << std::dec << ")" << std::endl;
  std::cerr << "  Active Cores:     ";
  for (int i = 0; i < num_threads; ++i) {
    if (i > 0) std::cerr << ", ";
    std::cerr << get_core_for_thread(USER_CORE_MASK, i);
  }
  std::cerr << std::endl;
  std::cerr << "  Threads:          " << num_threads << std::endl;
  std::cerr << "  Time Period:      " << TIME_PERIOD_SEC << " sec" << std::endl;
  std::cerr << "  Total Runtime:    " << TOTAL_RUNTIME_SEC << " sec" << std::endl;
  std::cerr << "  Block Size:       " << BLOCK_SIZE << " bytes" << std::endl;
  std::cerr << "  Blocks per Chunk: " << blocks_per_chunk << std::endl;
  std::cerr << "  Buffer Size:      " << (BUFFER_SIZE / (1024*1024)) << " MiB" << std::endl;
  std::cerr << "  Max Blocks:       " << MAX_BLOCKS << std::endl;
#ifdef USE_NONTEMPORAL
  std::cerr << "  Mode:          Non-Temporal" << std::endl;
#else
  std::cerr << "  Mode:          Cached" << std::endl;
#endif
#ifdef PREFETCH_NEXT
  std::cerr << "  Prefetch:      Enabled (dist=" << PF_DIST_BLOCKS << ", every=" << PF_EVERY << ")" << std::endl;
#else
  std::cerr << "  Prefetch:      Disabled" << std::endl;
#endif
  std::cerr << "========================================" << std::endl;
  std::cerr.flush();

  omp_set_num_threads(num_threads);
  std::cerr << "OpenMP threads set to: " << num_threads << std::endl;
  std::cerr.flush();

  // Use static aligned buffers
  std::cerr << "Using static aligned buffers..." << std::endl;
  std::uint8_t* write_buf = write_buf_global;
  std::uint8_t* read_buf = read_buf_global;
  
  std::cerr << "  Write buffer at: " << static_cast<void*>(write_buf) << std::endl;
  std::cerr << "  Read buffer at:  " << static_cast<void*>(read_buf) << std::endl;
  std::cerr << "  Alignment check: write=" << (reinterpret_cast<std::uintptr_t>(write_buf) % 4096)
            << ", read=" << (reinterpret_cast<std::uintptr_t>(read_buf) % 4096) << " (should be 0)" << std::endl;

  std::cerr << "Initializing buffers..." << std::endl;
  std::cerr.flush();
#ifdef RANDOMIZE_ALLOC
  fill_random(read_buf, BUFFER_SIZE, 1);
  fill_random(write_buf, BUFFER_SIZE, 2);
  std::cerr << "  Buffers filled with random data" << std::endl;
#else
  fill_zeros(read_buf, BUFFER_SIZE);
  fill_zeros(write_buf, BUFFER_SIZE);
  std::cerr << "  Buffers zero-initialized" << std::endl;
#endif
  std::cerr.flush();

  const std::uint64_t max_blocks = MAX_BLOCKS;
  const double period_sec = TIME_PERIOD_SEC;

  std::cerr << "========================================" << std::endl;
  std::cerr << "Starting benchmarks..." << std::endl;
  std::cerr << "========================================" << std::endl;
  std::cerr.flush();

  const double total_runtime = TOTAL_RUNTIME_SEC;
  const std::uint64_t freq = get_frequency();
  const std::uint64_t target_ticks = (std::uint64_t)(period_sec * freq);
  
  // Array to store measurement results per thread
  std::vector<MeasurementResult> measurement_results;

  // Atomic flags for benchmark loop control (shared across all threads)
  std::atomic<bool> write_continue{true};
  std::atomic<bool> read_continue{true};
  std::atomic<bool> copy_continue{true};

  double write_start = 0.0;
  double read_start = 0.0;
  double copy_start = 0.0;
  double write_end = 0.0;
  double read_end = 0.0;
  double copy_end = 0.0;

  #pragma omp parallel
  {
    const int tid = omp_get_thread_num();
    const int nthr = omp_get_num_threads();
    const int target_cpu = get_core_for_thread(USER_CORE_MASK, tid);
    
    // Retry CPU pinning for up to 10 seconds
    const double pin_timeout = 10.0;
    const double pin_start = get_time();
    bool pinned_successfully = false;
    int actual_cpu = -1;
    
    while ((get_time() - pin_start) < pin_timeout) {
      pin_to_cpu(target_cpu);
      actual_cpu = sched_getcpu();
      
      if (actual_cpu == target_cpu) {
        pinned_successfully = true;
        break;
      }
      
      // Small delay before retry
      usleep(1000); // 1ms
    }
    
    #pragma omp critical
    {
      if (pinned_successfully) {
        std::cerr << "Thread " << tid << "/" << nthr << " pinned to CPU " << actual_cpu << " (target: " << target_cpu << ")" << std::endl;
      } else {
        std::cerr << "Thread " << tid << "/" << nthr << " FAILED to pin to CPU " << target_cpu << " (actual: " << actual_cpu << ")" << std::endl;
      }
      std::cerr.flush();
    }
    
    #pragma omp barrier

    // Thread-local iteration counters
    std::uint64_t write_count = 0;
    std::uint64_t read_count = 0;
    std::uint64_t copy_count = 0;

    const std::size_t thread_offset = (tid * max_blocks / num_threads);
    std::uint8_t* w = write_buf + thread_offset * BLOCK_SIZE;
    const std::uint8_t* r = read_buf + thread_offset * BLOCK_SIZE;
    const std::uint64_t thread_max_blocks = max_blocks / num_threads;
    
    // Persistent chunk index across all benchmarks to avoid cached memory
    std::size_t chunk_start_idx = 0;

    // ===== WRITE BENCHMARK =====
    if (tid == 0) {
      #pragma omp critical
      {
        std::cerr << "\n[WRITE] Running for " << total_runtime << " seconds..." << std::endl;
        std::cerr.flush();
      }
    }
    
    
    #pragma omp single
    {
      write_continue.store(true);
      write_start = get_time();
    }
    
    // Run continuously for total_runtime seconds, processing one chunk at a time
    while (true) {

      #ifdef USE_STRIDED_BARRIER
      if (!write_continue.load(std::memory_order_acquire)) {
        break;
      }
      #endif
      
      std::uint64_t start_tick, end_tick;
      const std::uint64_t iters = chunk_write_benchmark(w, thread_max_blocks, start_tick, end_tick, chunk_start_idx, blocks_per_chunk);
      
      // Accumulate iterations
      write_count += iters;
      
      #ifdef USE_STRIDED_BARRIER
      // Single thread checks exit condition to ensure all threads exit together
      #pragma omp single nowait
      {
        if ((get_time() - write_start) >= total_runtime) {
          write_continue.store(false);
        }
      }
      #pragma omp barrier
      #else
      // Each thread checks independently
      if ((get_time() - write_start) >= total_runtime) {
        break;
      }
      #endif
    }

    // Commit writes
    __asm__ __volatile__("dsb ishst" ::: "memory");
    
    #pragma omp barrier
    
    #pragma omp single
    {
      write_end = get_time();
    }
    
    // Store thread's write result
    #pragma omp critical
    {
      measurement_results.push_back({
        Operation::WRITE,
        tid,
        write_count,
        write_start,
        write_end,
        blocks_per_chunk,
        chunk_start_idx
      });
    }
    
    if (tid == 0) {
      #pragma omp critical
      {
        std::cerr << "[WRITE] Completed" << std::endl;
        std::cerr << "[WRITE] Per-thread iteration counts:" << std::endl;
        for (const auto& result : measurement_results) {
          if (result.op == Operation::WRITE) {
            std::cerr << "  Thread " << result.tid << ": " << result.total_iterations << " iterations" << std::endl;
          }
        }
        std::cerr.flush();
      }
    }

    // ===== READ BENCHMARK =====
    if (tid == 0) {
      #pragma omp critical
      {
        std::cerr << "\n[READ] Running for " << total_runtime << " seconds..." << std::endl;
        std::cerr.flush();
      }
    }
    
    
    #pragma omp single
    {
      read_continue.store(true);
      read_start = get_time();
    }
    
    // Run continuously for total_runtime seconds, processing one chunk at a time
    while (true) {

      #ifdef USE_STRIDED_BARRIER
      if (!read_continue.load(std::memory_order_acquire)) {
        break;
      }
      #endif
      
      std::uint64_t start_tick, end_tick;
      const std::uint64_t iters = chunk_read_benchmark(r, thread_max_blocks, start_tick, end_tick, chunk_start_idx, blocks_per_chunk);
      
      // Accumulate iterations
      read_count += iters;
      
      #ifdef USE_STRIDED_BARRIER
      // Single thread checks exit condition to ensure all threads exit together
      #pragma omp single nowait
      {
        if ((get_time() - read_start) >= total_runtime) {
          read_continue.store(false);
        }
      }
      #pragma omp barrier
      #else
      // Each thread checks independently
      if ((get_time() - read_start) >= total_runtime) {
        break;
      }
      #endif
    }
    
    #pragma omp barrier
    
    #pragma omp single
    {
      read_end = get_time();
    }
    
    // Store thread's read result
    #pragma omp critical
    {
      measurement_results.push_back({
        Operation::READ,
        tid,
        read_count,
        read_start,
        read_end,
        blocks_per_chunk,
        chunk_start_idx
      });
    }
    
    if (tid == 0) {
      #pragma omp critical
      {
        std::cerr << "[READ] Completed" << std::endl;
        std::cerr << "[READ] Per-thread iteration counts:" << std::endl;
        for (const auto& result : measurement_results) {
          if (result.op == Operation::READ) {
            std::cerr << "  Thread " << result.tid << ": " << result.total_iterations << " iterations" << std::endl;
          }
        }
        std::cerr.flush();
      }
    }

    // ===== COPY BENCHMARK =====
    if (tid == 0) {
      #pragma omp critical
      {
        std::cerr << "\n[COPY] Running for " << total_runtime << " seconds..." << std::endl;
        std::cerr.flush();
      }
    }
    
    
    #pragma omp single
    {
      copy_continue.store(true);
      copy_start = get_time();
    }
    
    // Run continuously for total_runtime seconds, processing one chunk at a time
    while (true) {

      #ifdef USE_STRIDED_BARRIER
      if (!copy_continue.load(std::memory_order_acquire)) {
        break;
      }
      #endif
      
      std::uint64_t start_tick, end_tick;
      const std::uint64_t iters = chunk_copy_benchmark(w, r, thread_max_blocks, start_tick, end_tick, chunk_start_idx, blocks_per_chunk);
      
      // Accumulate iterations
      copy_count += iters;
      
      #ifdef USE_STRIDED_BARRIER
      // Single thread checks exit condition to ensure all threads exit together
      #pragma omp single nowait
      {
        if ((get_time() - copy_start) >= total_runtime) {
          copy_continue.store(false);
        }
      }
      #pragma omp barrier
      #else
      // Each thread checks independently
      if ((get_time() - copy_start) >= total_runtime) {
        break;
      }
      #endif
    }

    // Commit writes
    __asm__ __volatile__("dsb ish" ::: "memory");
    
    #pragma omp barrier

    #pragma omp single
    {
      copy_end = get_time();
    }
    
    // Store thread's copy result
    #pragma omp critical
    {
      measurement_results.push_back({
        Operation::COPY,
        tid,
        copy_count,
        copy_start,
        copy_end,
        blocks_per_chunk,
        chunk_start_idx
      });
    }
    
    if (tid == 0) {
      #pragma omp critical
      {
        std::cerr << "[COPY] Completed" << std::endl;
        std::cerr << "[COPY] Per-thread iteration counts:" << std::endl;
        for (const auto& result : measurement_results) {
          if (result.op == Operation::COPY) {
            std::cerr << "  Thread " << result.tid << ": " << result.total_iterations << " iterations" << std::endl;
          }
        }
        std::cerr.flush();
      }
    }
  }  // End of parallel region

  // ===== OUTPUT CSV RESULTS =====
  std::cerr << "\nGenerating CSV output..." << std::endl;
  std::cerr.flush();

  // CSV header
  std::cout << "core_mask,op,tid,total_iterations,start_time,end_time,blocks_per_chunk,chunk_idx\n";
  
  // Output all measurement results
  for (const auto& result : measurement_results) {
    std::cout << "0b" << std::bitset<8>(USER_CORE_MASK) << "," 
              << operation_to_string(result.op) << "," << result.tid << "," << result.total_iterations << "," 
              << std::fixed << std::setprecision(9) << result.start_time << "," << result.end_time << "," 
              << result.blocks_per_chunk << "," << result.chunk_idx << "\n";
  }
  
  std::cout.flush();

  std::cerr << "\n========================================" << std::endl;
  std::cerr << "All benchmarks completed successfully!" << std::endl;
  std::cerr << "========================================" << std::endl;
  std::cerr.flush();

  return 0;
}

