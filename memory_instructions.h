#ifndef MEMORY_INSTRUCTIONS_H
#define MEMORY_INSTRUCTIONS_H

#include <cstdint>
#include <cstddef>

#define FORCE_INLINE __attribute__((always_inline)) inline

// ============================================================================
// ARM64 NEON Assembly Inline Functions - Cached Operations
// ============================================================================


FORCE_INLINE void READ_CACHED_512_BYTES(const void* addr) {
  __asm__ __volatile__(
    "ldp q15, q16, [%[p]]        \n\t"
    "ldp q15, q16, [%[p], #32]   \n\t"
    "ldp q15, q16, [%[p], #64]   \n\t"
    "ldp q15, q16, [%[p], #96]   \n\t"
    "ldp q15, q16, [%[p], #128]  \n\t"
    "ldp q15, q16, [%[p], #160]  \n\t"
    "ldp q15, q16, [%[p], #192]  \n\t"
    "ldp q15, q16, [%[p], #224]  \n\t"
    "ldp q15, q16, [%[p], #256]  \n\t"
    "ldp q15, q16, [%[p], #288]  \n\t"
    "ldp q15, q16, [%[p], #320]  \n\t"
    "ldp q15, q16, [%[p], #352]  \n\t"
    "ldp q15, q16, [%[p], #384]  \n\t"
    "ldp q15, q16, [%[p], #416]  \n\t"
    "ldp q15, q16, [%[p], #448]  \n\t"
    "ldp q15, q16, [%[p], #480]  \n\t"
    :
    : [p] "r"(addr)
    : "v15","v16","memory"
  );
}

FORCE_INLINE void WRITE_CACHED_512_BYTES(void* dst) {
  __asm__ __volatile__(
    "stp q15, q16, [%[d]]        \n\t"
    "stp q15, q16, [%[d], #32]   \n\t"
    "stp q15, q16, [%[d], #64]   \n\t"
    "stp q15, q16, [%[d], #96]   \n\t"
    "stp q15, q16, [%[d], #128]  \n\t"
    "stp q15, q16, [%[d], #160]  \n\t"
    "stp q15, q16, [%[d], #192]  \n\t"
    "stp q15, q16, [%[d], #224]  \n\t"
    "stp q15, q16, [%[d], #256]  \n\t"
    "stp q15, q16, [%[d], #288]  \n\t"
    "stp q15, q16, [%[d], #320]  \n\t"
    "stp q15, q16, [%[d], #352]  \n\t"
    "stp q15, q16, [%[d], #384]  \n\t"
    "stp q15, q16, [%[d], #416]  \n\t"
    "stp q15, q16, [%[d], #448]  \n\t"
    "stp q15, q16, [%[d], #480]  \n\t"
    :
    : [d] "r"(dst)
    : "v15","v16","memory"
  );
}

FORCE_INLINE void COPY_CACHED_512_BYTES(void* dst, const void* src) {
  __asm__ __volatile__(
    "ldp q15, q16, [%[s]]        \n\t"  "stp q15, q16, [%[d]]        \n\t"
    "ldp q15, q16, [%[s], #32]   \n\t"  "stp q15, q16, [%[d], #32]   \n\t"
    "ldp q15, q16, [%[s], #64]   \n\t"  "stp q15, q16, [%[d], #64]   \n\t"
    "ldp q15, q16, [%[s], #96]   \n\t"  "stp q15, q16, [%[d], #96]   \n\t"
    "ldp q15, q16, [%[s], #128]  \n\t"  "stp q15, q16, [%[d], #128]  \n\t"
    "ldp q15, q16, [%[s], #160]  \n\t"  "stp q15, q16, [%[d], #160]  \n\t"
    "ldp q15, q16, [%[s], #192]  \n\t"  "stp q15, q16, [%[d], #192]  \n\t"
    "ldp q15, q16, [%[s], #224]  \n\t"  "stp q15, q16, [%[d], #224]  \n\t"
    "ldp q15, q16, [%[s], #256]  \n\t"  "stp q15, q16, [%[d], #256]  \n\t"
    "ldp q15, q16, [%[s], #288]  \n\t"  "stp q15, q16, [%[d], #288]  \n\t"
    "ldp q15, q16, [%[s], #320]  \n\t"  "stp q15, q16, [%[d], #320]  \n\t"
    "ldp q15, q16, [%[s], #352]  \n\t"  "stp q15, q16, [%[d], #352]  \n\t"
    "ldp q15, q16, [%[s], #384]  \n\t"  "stp q15, q16, [%[d], #384]  \n\t"
    "ldp q15, q16, [%[s], #416]  \n\t"  "stp q15, q16, [%[d], #416]  \n\t"
    "ldp q15, q16, [%[s], #448]  \n\t"  "stp q15, q16, [%[d], #448]  \n\t"
    "ldp q15, q16, [%[s], #480]  \n\t"  "stp q15, q16, [%[d], #480]  \n\t"
    :
    : [d] "r"(dst), [s] "r"(src)
    : "v15","v16","memory"
  );
}

// ============================================================================
// ARM64 NEON Assembly Inline Functions - Non-Temporal Operations
// ============================================================================

FORCE_INLINE void READ_NONTEMPORAL_512_BYTES(const void* addr) {
  __asm__ __volatile__(
    "ldnp q16, q17, [%[p], #0]   \n\t"
    "ldnp q16, q17, [%[p], #32]  \n\t"
    "ldnp q16, q17, [%[p], #64]  \n\t"
    "ldnp q16, q17, [%[p], #96]  \n\t"
    "ldnp q16, q17, [%[p], #128] \n\t"
    "ldnp q16, q17, [%[p], #160] \n\t"
    "ldnp q16, q17, [%[p], #192] \n\t"
    "ldnp q16, q17, [%[p], #224] \n\t"
    "ldnp q16, q17, [%[p], #256] \n\t"
    "ldnp q16, q17, [%[p], #288] \n\t"
    "ldnp q16, q17, [%[p], #320] \n\t"
    "ldnp q16, q17, [%[p], #352] \n\t"
    "ldnp q16, q17, [%[p], #384] \n\t"
    "ldnp q16, q17, [%[p], #416] \n\t"
    "ldnp q16, q17, [%[p], #448] \n\t"
    "ldnp q16, q17, [%[p], #480] \n\t"
    :
    : [p] "r"(addr)
    : "v16","v17","memory"
  );
}

FORCE_INLINE void WRITE_NONTEMPORAL_512_BYTES(void* dst) noexcept {
  __asm__ __volatile__(
    "stnp q16, q17, [%[d], #0]   \n\t"
    "stnp q16, q17, [%[d], #32]  \n\t"
    "stnp q16, q17, [%[d], #64]  \n\t"
    "stnp q16, q17, [%[d], #96]  \n\t"
    "stnp q16, q17, [%[d], #128] \n\t"
    "stnp q16, q17, [%[d], #160] \n\t"
    "stnp q16, q17, [%[d], #192] \n\t"
    "stnp q16, q17, [%[d], #224] \n\t"
    "stnp q16, q17, [%[d], #256] \n\t"
    "stnp q16, q17, [%[d], #288] \n\t"
    "stnp q16, q17, [%[d], #320] \n\t"
    "stnp q16, q17, [%[d], #352] \n\t"
    "stnp q16, q17, [%[d], #384] \n\t"
    "stnp q16, q17, [%[d], #416] \n\t"
    "stnp q16, q17, [%[d], #448] \n\t"
    "stnp q16, q17, [%[d], #480] \n\t"
    :
    : [d] "r"(dst)
    : "v16","v17","memory"
  );
}

FORCE_INLINE void COPY_NONTEMPORAL_512_BYTES(void* dst, const void* src) {
  __asm__ __volatile__(
    "ldnp q16, q17, [%[s], #0]    \n\t"  "stnp q16, q17, [%[d], #0]    \n\t"
    "ldnp q16, q17, [%[s], #32]   \n\t"  "stnp q16, q17, [%[d], #32]   \n\t"
    "ldnp q16, q17, [%[s], #64]   \n\t"  "stnp q16, q17, [%[d], #64]   \n\t"
    "ldnp q16, q17, [%[s], #96]   \n\t"  "stnp q16, q17, [%[d], #96]   \n\t"
    "ldnp q16, q17, [%[s], #128]  \n\t"  "stnp q16, q17, [%[d], #128]  \n\t"
    "ldnp q16, q17, [%[s], #160]  \n\t"  "stnp q16, q17, [%[d], #160]  \n\t"
    "ldnp q16, q17, [%[s], #192]  \n\t"  "stnp q16, q17, [%[d], #192]  \n\t"
    "ldnp q16, q17, [%[s], #224]  \n\t"  "stnp q16, q17, [%[d], #224]  \n\t"
    "ldnp q16, q17, [%[s], #256]  \n\t"  "stnp q16, q17, [%[d], #256]  \n\t"
    "ldnp q16, q17, [%[s], #288]  \n\t"  "stnp q16, q17, [%[d], #288]  \n\t"
    "ldnp q16, q17, [%[s], #320]  \n\t"  "stnp q16, q17, [%[d], #320]  \n\t"
    "ldnp q16, q17, [%[s], #352]  \n\t"  "stnp q16, q17, [%[d], #352]  \n\t"
    "ldnp q16, q17, [%[s], #384]  \n\t"  "stnp q16, q17, [%[d], #384]  \n\t"
    "ldnp q16, q17, [%[s], #416]  \n\t"  "stnp q16, q17, [%[d], #416]  \n\t"
    "ldnp q16, q17, [%[s], #448]  \n\t"  "stnp q16, q17, [%[d], #448]  \n\t"
    "ldnp q16, q17, [%[s], #480]  \n\t"  "stnp q16, q17, [%[d], #480]  \n\t"
    :
    : [d] "r"(dst), [s] "r"(src)
    : "v16","v17","memory"
  );
}

// ============================================================================
// Prefetch Functions
// ============================================================================

#ifdef USE_NONTEMPORAL
  #define PRFM_READ_HINT "pldl2strm"
  #define PRFM_WRITE_HINT "pstl2strm"
#else
  #define PRFM_READ_HINT "pldl2keep"
  #define PRFM_WRITE_HINT "pstl2keep"
#endif

FORCE_INLINE void PREFETCH_READ(const void* addr) {
  __asm__ __volatile__("prfm " PRFM_READ_HINT ", [%[a]]" :: [a]"r"(addr));
}

FORCE_INLINE void PREFETCH_WRITE(const void* addr) {
  __asm__ __volatile__("prfm " PRFM_WRITE_HINT ", [%[a]]" :: [a]"r"(addr));
}

FORCE_INLINE void PREFETCH_N_BYTES_READ_HINT(const void* addr, std::size_t nbytes, std::size_t line_bytes = 64) noexcept {
  const char* p = static_cast<const char*>(addr);
  const char* end = p + nbytes;

  // guard against zero/odd inputs
  if (line_bytes == 0) line_bytes = 64;
  for (const char* cur = p; cur < end; cur += line_bytes) {
    PREFETCH_READ(cur);
  }
}

FORCE_INLINE void PREFETCH_N_BYTES_WRITE_HINT(const void* addr, std::size_t nbytes, std::size_t line_bytes = 64) noexcept {
  const char* p = static_cast<const char*>(addr);
  const char* end = p + nbytes;

  // guard against zero/odd inputs
  if (line_bytes == 0) line_bytes = 64;
  for (const char* cur = p; cur < end; cur += line_bytes) {
    PREFETCH_WRITE(cur);
  }
}

// ============================================================================
// Timer Functions
// ============================================================================

FORCE_INLINE std::uint64_t get_timestamp()
{
  std::uint64_t timestamp = 0;
  __asm__ __volatile__(
      // Get the current timestamp, saving to %[timestamp]
      "mrs %[timestamp], cntvct_el0"
      : [timestamp] "=r"(timestamp)
      :
      :);
  return timestamp;
}

FORCE_INLINE std::uint64_t get_frequency()
{
    std::uint64_t hz;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(hz));
    return hz;
}

FORCE_INLINE double get_time()
{
  static const double hz = (double)get_frequency();
  return (double)get_timestamp() / hz;
}

FORCE_INLINE double ticks_to_seconds(std::uint64_t ticks)
{
  static const double hz = (double)get_frequency();
  return (double)ticks / hz;
}

#endif // MEMORY_INSTRUCTIONS_H

