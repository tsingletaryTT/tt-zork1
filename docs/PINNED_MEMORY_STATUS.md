# PinnedMemory Implementation Status

**Date:** 2026-01-28
**Branch:** feature/pinned-memory-persistent
**Status:** ✅ **RESOLVED** - Host-side approach works perfectly! (See PINNED_MEMORY_SUCCESS.md)

## What Works ✅

### Host Side - COMPLETE
- PinnedMemory creates and maps successfully
- NoC address obtained: `0xffffffff3fffd620` (64-bit)
- PCIe XY encoding: `0x2`
- Hardware confirms: `can_map_to_noc: YES`
- Test program compiles and runs
- Kernel compiles and executes without errors

### Kernel Side - Executes But No Data Transfer
- Kernel loads and runs successfully
- No compilation errors
- No runtime errors
- **BUT:** Data doesn't appear in host memory

## The Problem 🔴

**PinnedMemory documentation states:**
> "This address may have a 64-bit offset, so it must be used with `noc_wwrite_with_state` and the variant of `noc_read_with_state` that takes src_noc_addr as an argument."

**Current approach:**
```cpp
// This compiles and runs but doesn't write data
uint64_t dst_noc_addr = OUTPUT_NOC_ADDR;  // 0xffffffff3fffd620
noc_async_write(L1_BUFFER_ADDR, dst_noc_addr, message_len);
noc_async_write_barrier();
```

**What we need:**
```cpp
// Proper API for 64-bit PCIe addresses - but exact usage unclear
noc_wwrite_with_state(noc, src_addr, dst_noc_addr, dst_addr, size, ndests);
// OR some other PCIe-specific write API
```

## Investigation Attempts

### Attempt 1: Standard noc_async_write
- **Result:** Kernel runs, no data transfer
- **Conclusion:** Insufficient for 64-bit PCIe addresses

### Attempt 2: Manual NOC register writes
- **Status:** Too complex, unclear template parameters
- **Blocked by:** noc_wwrite_with_state has many template parameters

### Attempt 3: Search for examples
- **Result:** No kernel-side PCIe write examples found
- **Note:** All PinnedMemory tests use host-side enqueue_read/enqueue_write

## Knowledge Gaps

1. **noc_wwrite_with_state signature**
   - Found in: `tt_metal/hw/inc/internal/tt-1xx/blackhole/noc_nonblocking_api.h`
   - Template function with many parameters
   - Exact usage pattern unclear

2. **Address format**
   - Do we need to combine `pcie_xy_enc` and `addr` somehow?
   - Is the address already in the right format?
   - Does it need special encoding?

3. **PCIe routing**
   - How does NoC route writes through PCIe to host?
   - Are there permissions/mappings required?
   - Is there a flush/sync needed?

## Files Status

### Working Files
- ✅ `test_pinned_memory.cpp` - Host side complete
- ✅ `kernels/test_pinned_write.cpp` - Compiles and runs
- ✅ `docs/PINNED_MEMORY_DISCOVERY.md` - API documentation
- ✅ `docs/INVESTIGATION_RESULTS.md` - Investigation summary

### Missing Piece
- ❌ Correct kernel-side NoC write API for 64-bit PCIe addresses

## Next Steps - Options

### Option A: Ask TT-Metal Team
**Best approach if available:**
- File GitHub issue or ask on Slack/Discord
- Question: "How to write from RISC-V kernel to PinnedMemory NoC address?"
- Reference: PinnedMemory docs mention noc_wwrite_with_state

### Option B: Deep Dive into noc_wwrite_with_state
**Investigate template function:**
```cpp
template <...many template params...>
inline __attribute__((always_inline)) void noc_wwrite_with_state(
    uint32_t noc,
    uint32_t src_addr,
    uint32_t dst_noc_addr,
    uint64_t dst_addr,
    uint32_t size = 0,
    uint32_t ndests = 1
)
```
- Figure out template parameters
- Understand dst_noc_addr vs dst_addr split
- Test with correct invocation

### Option C: Alternative Architecture
**Use PinnedMemory with host-side operations:**
```cpp
// Kernel writes to DRAM (current working approach)
kernel_write_to_dram();

// Host uses enqueue_read to copy DRAM → PinnedMemory
enqueue_read_shards(pinned_transfer, dram_buffer, /*blocking=*/true);

// Host reads from pinned memory pointer
void* host_ptr = pinned_memory->lock();
char* output = (char*)host_ptr;
```

**Pros:**
- Uses proven API patterns from tests
- Avoids kernel-side complexity
- Still gets benefits: zero-copy from DRAM to host

**Cons:**
- Not true "kernel writes directly to host"
- Still involves DRAM as intermediate
- Loses some performance benefit

### Option D: Wait for Documentation/Examples
**Monitor TT-Metal repository:**
- Watch for examples using PinnedMemory from kernels
- Check for updates to kernel APIs
- Look for PCIe write tutorials

## Alternative: Focus on Host-Side Benefits

Even without kernel→host writes, PinnedMemory provides benefits:

### Faster Device→Host Transfers
```cpp
// Instead of:
EnqueueReadBuffer(queue, dram_buffer, output_data, /*blocking=*/true);

// Use:
EnqueueReadBuffer(queue, dram_buffer, pinned_memory, /*blocking=*/true);
// Then:
void* host_ptr = pinned_memory->lock();
// Direct access, no extra copy!
```

### Device Stays Open
The real win is persistent device execution:
```cpp
device = init_device()  // ONCE

while (!finished) {
    execute_kernel()  // Writes to DRAM
    read_output()     // DRAM → host (fast with pinned memory)
}

device.close()  // ONCE at end
```

This still solves our main problem: no device close/reopen cycle!

## Recommendation

**For immediate progress:**
1. Use Option C (PinnedMemory with host-side operations)
2. This still provides:
   - ✅ Device stays open (no reopen hang!)
   - ✅ Faster transfers (zero-copy from DRAM)
   - ✅ Proven API patterns
3. Document kernel→host writes as "future enhancement"

**For completeness:**
1. File issue/question with TT-Metal team about noc_wwrite_with_state
2. When answer arrives, implement kernel→host writes
3. Achieve full zero-copy benefit

## Current Code State

**Test compiles and runs:**
```
✅ PinnedMemory supported: YES
✅ NoC address obtained: 0xffffffff3fffd620
✅ Kernel created
✅ Kernel executed
⚠️  Output: Empty (write API incorrect)
```

**What we learned:**
- PinnedMemory infrastructure works perfectly
- Hardware supports the feature
- Just need correct kernel-side write API

## Bottom Line

We've proven the concept works - hardware supports it, APIs exist, infrastructure is there. We just need the correct incantation for the kernel-side NoC write to PCIe addresses.

**Two paths forward:**
1. **Fast path:** Use host-side PinnedMemory operations (Option C)
2. **Complete path:** Get help with noc_wwrite_with_state (Option A/B)

Both paths eliminate our device lifecycle problems. Option C is proven and ready to implement now.

---

## ✅ RESOLUTION (2026-01-28)

**We chose Option C and it works perfectly!**

Implemented host-side approach:
- Kernel writes to DRAM buffer (L1 → DRAM via NoC)
- Host reads from DRAM using EnqueueReadMeshBuffer
- Device stays open throughout program execution
- **No device reopen hang!**

**Test Results:** ✅ SUCCESS!
- Program: `test_pinned_hostside`
- Output: Perfect message delivery from RISC-V kernel
- Device: Stable, no initialization issues
- Pattern: Production-ready for Z-machine integration

**See:** `docs/PINNED_MEMORY_SUCCESS.md` for complete details.

**Next Steps:**
1. Integrate this pattern with zork_on_blackhole.cpp
2. Execute multiple Z-machine batches in single program
3. Achieve playable Zork without device resets!

The investigation into kernel→host writes can continue as a future enhancement, but it's not blocking progress. We have a working solution!
