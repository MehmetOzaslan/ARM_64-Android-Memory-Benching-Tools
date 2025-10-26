#include <atomic>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <omp.h>

#include "memory_settings.h"

using u64 = std::uint64_t;

// ============================================================================
// Configuration: Static aligned buffers
// ============================================================================

// Static aligned buffers
alignas(4096) static std::uint8_t read_buf_global [BUFFER_SIZE];
alignas(4096) static std::uint8_t write_buf_global[BUFFER_SIZE];

// #define USE_STRIDED_BARRIER

// ============================================================================
// Measurement Result Structure
// ============================================================================

struct MeasurementResult {
  const char* op;              // "write", "read", or "copy"
  std::uint64_t period_ticks;
  std::uint64_t iterations;
  std::uint64_t end_timestamp_ticks;
  int tid;
  std::size_t chunk_idx;       // starting chunk index for this measurement
};

// ============================================================================
// Time-based Benchmark Functions
// ============================================================================

// Write benchmark: process one chunk of blocks
// Returns number of blocks processed, sets actual start/end ticks
FORCE_INLINE std::uint64_t
chunk_write_benchmark(std::uint8_t*   w, const std::uint64_t max_blocks, 
                      std::uint64_t& start_tick, std::uint64_t& end_tick,
                      std::size_t& chunk_start_idx) {
  start_tick = get_timestamp();
  std::uint64_t iterations = 0;  // Total blocks processed in this chunk

  // Process one chunk of consecutive blocks
  const std::size_t chunk_end = std::min(chunk_start_idx + (std::size_t)BLOCKS_PER_CHUNK, (std::size_t)max_blocks);
  
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
  chunk_start_idx += BLOCKS_PER_CHUNK;
  if (chunk_start_idx >= max_blocks) chunk_start_idx = 0;

  end_tick = get_timestamp();
  return iterations;
}

// Read benchmark: process one chunk of blocks
// Returns number of blocks processed, sets actual start/end ticks
FORCE_INLINE std::uint64_t
chunk_read_benchmark(const std::uint8_t*   r, const std::uint64_t max_blocks, 
                     std::uint64_t& start_tick, std::uint64_t& end_tick,
                     std::size_t& chunk_start_idx) {
  start_tick = get_timestamp();
  std::uint64_t iterations = 0;  // Total blocks processed in this chunk

  // Process one chunk of consecutive blocks
  const std::size_t chunk_end = std::min(chunk_start_idx + (std::size_t)BLOCKS_PER_CHUNK, (std::size_t)max_blocks);
  
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
  chunk_start_idx += BLOCKS_PER_CHUNK;
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
                     std::size_t& chunk_start_idx) {
  start_tick = get_timestamp();
  std::uint64_t iterations = 0;  // Total blocks processed in this chunk

  // Process one chunk of consecutive blocks
  const std::size_t chunk_end = std::min(chunk_start_idx + (std::size_t)BLOCKS_PER_CHUNK, (std::size_t)max_blocks);
  
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
  chunk_start_idx += BLOCKS_PER_CHUNK;
  if (chunk_start_idx >= max_blocks) chunk_start_idx = 0;

  end_tick = get_timestamp();
  return iterations;
}

// ============================================================================
// Main
// ============================================================================

int main() {
  // Keep stderr unbuffered for progress messages
  setvbuf(stderr, nullptr, _IONBF, 0);
  std::cerr.setf(std::ios::unitbuf);

  // Determine number of threads from core mask
  const int num_threads = count_cores_in_mask(USER_CORE_MASK);
  
  std::cerr << "========================================" << std::endl;
  std::cerr.flush();
  std::cerr << "Instantaneous Stream Benchmark" << std::endl;
  std::cerr << "========================================" << std::endl;
  std::cerr << "Configuration:" << std::endl;
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
  std::cerr << "  Blocks per Chunk: " << BLOCKS_PER_CHUNK << std::endl;
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
  
  // Vector to store all measurements from all threads
  std::vector<MeasurementResult> all_results;
  all_results.reserve(5000000);

  // Atomic flags for benchmark loop control (shared across all threads)
  std::atomic<bool> write_continue{true};
  std::atomic<bool> read_continue{true};
  std::atomic<bool> copy_continue{true};

  double write_start = 0.0;
  double read_start = 0.0;
  double copy_start = 0.0;

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
      
      // Small delay before retrying
      usleep(1000);  // 1ms
    }
    
    if (!pinned_successfully) {
      #pragma omp critical
      {
        std::cerr << "ERROR: Thread " << tid << " failed to pin to CPU " << target_cpu 
                  << " after " << pin_timeout << " seconds (running on CPU " << actual_cpu << ")" << std::endl;
        std::cerr.flush();
      }
      std::exit(1);
    }
    
    #pragma omp critical
    {
      std::cerr << "Thread " << tid << "/" << nthr << " successfully pinned to CPU " << target_cpu 
                << " (took " << (get_time() - pin_start) << "s)" << std::endl;
      std::cerr.flush();
    }
    
    #pragma omp barrier

    // Thread-local buffer for ALL measurements (write, read, copy)
    std::vector<MeasurementResult> thread_results;
    thread_results.reserve(3000000);  // Estimate for all three benchmarks

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
    while (write_continue.load(std::memory_order_acquire)) {
      const std::size_t chunk_idx = chunk_start_idx;  // capture before it advances
      std::uint64_t start_tick, end_tick;
      const std::uint64_t iters = chunk_write_benchmark(w, thread_max_blocks, start_tick, end_tick, chunk_start_idx);
      const std::uint64_t period_ticks = end_tick - start_tick;
      
      // Buffer the measurement
      thread_results.push_back({
        "write",
        period_ticks,
        iters,
        end_tick,
        tid,
        chunk_idx
      });
      
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
        write_continue.store(false);
      }
      #endif
    }

    // Commit writes
    __asm__ __volatile__("dsb ishst" ::: "memory");
    
    #pragma omp barrier
    
    if (tid == 0) {
      #pragma omp critical
      {
        std::cerr << "[WRITE] Completed" << std::endl;
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
    while (read_continue.load(std::memory_order_acquire)) {
      const std::size_t chunk_idx = chunk_start_idx;  // capture before it advances
      std::uint64_t start_tick, end_tick;
      const std::uint64_t iters = chunk_read_benchmark(r, thread_max_blocks, start_tick, end_tick, chunk_start_idx);
      const std::uint64_t period_ticks = end_tick - start_tick;
      
      // Buffer the measurement
      thread_results.push_back({
        "read",
        period_ticks,
        iters,
        end_tick,
        tid,
        chunk_idx
      });
      
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
        read_continue.store(false);
      }
      #endif
    }
    
    #pragma omp barrier
    
    if (tid == 0) {
      #pragma omp critical
      {
        std::cerr << "[READ] Completed" << std::endl;
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
    while (copy_continue.load(std::memory_order_acquire)) {
      const std::size_t chunk_idx = chunk_start_idx;  // capture before it advances
      std::uint64_t start_tick, end_tick;
      const std::uint64_t iters = chunk_copy_benchmark(w, r, thread_max_blocks, start_tick, end_tick, chunk_start_idx);
      const std::uint64_t period_ticks = end_tick - start_tick;
      
      // Buffer the measurement
      thread_results.push_back({
        "copy",
        period_ticks,
        iters,
        end_tick,
        tid,
        chunk_idx
      });
      
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
        copy_continue.store(false);
      }
      #endif
    }

    // Commit writes
    __asm__ __volatile__("dsb ish" ::: "memory");
    

    #pragma omp barrier

    
    if (tid == 0) {
      #pragma omp critical
      {
        std::cerr << "[COPY] Completed" << std::endl;
        std::cerr.flush();
      }
    }

    // ===== FLUSH THREAD RESULTS TO GLOBAL BUFFER =====
    // Only at the very end of ALL benchmarks do we merge thread-local results
    #pragma omp critical
    {
      all_results.insert(all_results.end(), thread_results.begin(), thread_results.end());
    }
  }  // End of parallel region

  // ===== OUTPUT ALL RESULTS =====
  std::cerr << "\nWriting " << all_results.size() << " measurements to output..." << std::endl;
  std::cerr.flush();

  // Build output string in memory first (much faster than individual cout calls)
  std::string output;
  output.reserve(all_results.size() * 60);  // Estimate ~60 chars per line
  
  // CSV header
  output += "op,period_ticks,iterations,end_timestamp_ticks,tid,chunk_idx\n";

  // Format all measurements into the string buffer
  char line_buffer[128];
  for (const auto& result : all_results) {
    snprintf(line_buffer, sizeof(line_buffer), 
             "%s,%lu,%lu,%lu,%d,%zu\n",
             result.op, result.period_ticks, result.iterations, 
             result.end_timestamp_ticks, result.tid, result.chunk_idx);
    output += line_buffer;
  }

  // Write everything at once
  std::cout << output;
  std::cout.flush();

  std::cerr << "\n========================================" << std::endl;
  std::cerr << "All benchmarks completed successfully!" << std::endl;
  std::cerr << "========================================" << std::endl;
  std::cerr.flush();

  return 0;
}

