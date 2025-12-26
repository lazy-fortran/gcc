# Metal Backend: Kernel Argument Passing Fix

## Issue #39 Resolution

### Problem Identified

The Metal backend kernel signature generation was incompatible with the GOMP
runtime's argument passing convention.

**Before (BROKEN)**:
```metal
[[kernel]] void kernel_name(
    device float* arg0 [[buffer(0)]],
    device float* arg1 [[buffer(1)]],  // Runtime never binds buffer(1)!
    float arg2,                         // Runtime never passes this!
    uint3 tid [[thread_position_in_grid]]
)
```

**Runtime behavior**:
- GOMP packs ALL arguments into device memory at address `dp`
- Plugin binds this memory as `buffer(0)` only
- Kernel expects separate buffer bindings → mismatch!

### Root Cause

Misunderstanding of GOMP argument passing model:

1. **NVPTX approach**: Single `.param` pointer to packed struct
2. **Metal (old)**: Tried to match OpenACC source-level API
3. **Correct approach**: Match GOMP's runtime behavior, not source API

### Solution Implemented

**After (FIXED)**:
```metal
[[kernel]] void kernel_name(
    constant void* _args_ptr [[buffer(0)]],  // Matches runtime binding
    uint3 tid [[thread_position_in_grid]],
    uint3 tgid [[threadgroup_position_in_grid]]
)
{
  // TODO: Unpack arguments from _args_ptr
  // Example for multiple arrays + scalar:
  // constant float** ptrs = (constant float**)_args_ptr;
  // device float* a = (device float*)ptrs[0];
  // device float* b = (device float*)ptrs[1];
  // float c = *((constant float*)(ptrs + 2));
}
```

### Changes Made

**File: gcc/config/metal/metal.cc**

1. **Removed `metal_write_kernel_arg()`**:
   - Old function generated per-parameter buffer bindings
   - Incompatible with GOMP's packed argument model

2. **Added `metal_write_kernel_params_unpacked()`**:
   - Generates single `constant void* _args_ptr [[buffer(0)]]` parameter
   - Matches GOMP runtime's packed argument buffer
   - Matches plugin's `enc->setBuffer(args_buffer, offset, 0)` binding

3. **Updated `metal_declare_function_name()`**:
   - Calls new parameter generation function
   - Simplified parameter emission logic

### Verification Status

**Signature Generation**: ✅ FIXED
- Kernel now receives single pointer parameter at buffer(0)
- Matches plugin binding behavior
- Matches GOMP argument packing model

**Argument Unpacking**: ⚠️ TODO
- Kernel receives pointer but doesn't use it yet
- Need to generate code to extract individual arguments
- Currently will compile but not execute correctly

**Required Next Steps**:
1. Generate argument unpacking code in kernel prologue
2. Map argument indices to correct types (pointer vs scalar)
3. Handle Fortran array descriptors if needed
4. Test with actual OpenACC code

### Compatibility

This fix maintains the NVPTX argument passing model:

| Aspect | NVPTX | Metal (Fixed) |
|--------|-------|---------------|
| Runtime packing | ✅ Single buffer | ✅ Single buffer |
| Plugin binding | ✅ Single param | ✅ buffer(0) |
| Kernel signature | ✅ Single .param | ✅ Single [[buffer(0)]] |
| Argument extraction | ✅ ld.param | ⚠️ TODO: Cast + dereference |

### Testing

To test this fix:

```fortran
program test_args
  implicit none
  integer, parameter :: n = 100
  real :: a(n), b(n), c
  integer :: i

  c = 3.14

  !$acc parallel loop
  do i = 1, n
    a(i) = b(i) + c
  end do
  !$acc end parallel loop
end program
```

**Expected MSL output**:
```metal
[[kernel]] void MAIN__._omp_fn.0(
    constant void* _args_ptr [[buffer(0)]],
    uint3 tid [[thread_position_in_grid]],
    uint3 tgid [[threadgroup_position_in_grid]]
)
{
  // Kernel body (argument unpacking TBD)
}
```

**Expected runtime behavior**:
1. GOMP allocates device buffer with [ptr_a, ptr_b, c]
2. Plugin finds buffer, binds as buffer(0)
3. Kernel receives pointer to buffer
4. Kernel unpacks: a=*(float**)args[0], b=*(float**)args[1], c=*(float*)&args[2]

### References

- **NVPTX implementation**: libgomp/plugin/plugin-nvptx.c:830-1072
  - `nvptx_exec()`: passes `kargs[0] = &dp`
  - Single parameter model proven working

- **GOMP argument packing**: libgomp/oacc-parallel.c:307
  - `devaddrs = (void **) tgt->tgt_start`
  - Runtime manages packed argument buffer

- **Issue**: #39 - Kernel argument passing verification

---

**Date**: 2025-12-26
**Status**: Signature fixed, unpacking pending
**Branch**: accelerate-metal
