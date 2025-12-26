# Metal Backend Kernel Argument Passing Analysis

## Issue #39: Verify kernel argument passing works correctly

### Current Implementation Flow

1. **Frontend (metal.cc)**:
   - `metal_write_kernel_arg()` generates kernel parameters
   - Pointers → `device type* argN [[buffer(N)]]` with incrementing indices
   - Scalars → `type argN` (no buffer attribute, no index increment)

2. **Runtime (plugin-metal.cpp)**:
   - `GOMP_OFFLOAD_openacc_exec()` receives `devaddrs` pointer
   - `metal_bind_kernel_arguments()` binds ONE buffer at index 0
   - Uses: `enc->setBuffer(args_buffer, offset, 0)`

### Problems Identified

#### Problem 1: Single Buffer Binding
**Current**: Only `buffer(0)` is bound, regardless of argument count
**Expected**: Each pointer argument should have its own buffer binding

**Example**:
```fortran
!$acc parallel loop
do i = 1, n
  a(i) = b(i) + c(i) + d
end do
```

**Generated kernel signature**:
```metal
[[kernel]] void kernel(
    device float* arg0 [[buffer(0)]],  // a
    device float* arg1 [[buffer(1)]],  // b
    device float* arg2 [[buffer(2)]],  // c - MISSING BINDING!
    float arg2,                         // d - scalar
    uint3 tid [[thread_position_in_grid]]
)
```

**Actual bindings**: Only `buffer(0)` is bound → kernel crashes or gets garbage

#### Problem 2: Scalar Argument Passing
**Current**: No mechanism to pass scalar values
**Expected**: Scalars should be passed directly or via a separate parameter buffer

Metal kernels can receive scalars in two ways:
1. **Direct parameters** (small scalars like int, float)
2. **Constant buffer** (for larger data)

The current implementation generates scalar parameters correctly but has no
runtime support to actually pass the values.

#### Problem 3: Array Descriptor Handling
**Status**: UNKNOWN - needs verification

OpenACC/OpenMP may pass Fortran array descriptors which need to be unwrapped
to base pointers. Current implementation assumes this happens upstream but
needs verification.

### Root Cause Analysis - CORRECTED

After examining NVPTX plugin implementation:

**NVPTX approach** (plugin-nvptx.c):
- Runtime packs ALL arguments into device memory
- Passes SINGLE pointer to kernel: `kargs[0] = &dp`
- Kernel receives: `.param .u64 %in_ar0` (single pointer parameter)
- Kernel loads arguments from memory via pointer dereference

**Metal current implementation**:
- Runtime: Correctly receives `devaddrs` pointer (same as NVPTX)
- Runtime: Correctly binds as `buffer(0)` (equivalent to NVPTX's single param)
- **BUG**: Kernel signature generation expects SEPARATE parameters, not packed struct

The disconnect is in `metal_write_kernel_arg()` which generates:
```metal
device float* arg0 [[buffer(0)]],
device float* arg1 [[buffer(1)]],  // WRONG - no buffer(1) bound!
float arg2                          // WRONG - not in struct!
```

Should generate (matching NVPTX model):
```metal
constant void* args [[buffer(0)]]  // Single pointer to packed args
```

Or with proper unpacking:
```metal
device kernel_args_t* args [[buffer(0)]]
// where kernel_args_t is a struct with all parameters
```

### Impact Assessment

**Currently working**:
- Single array argument kernels (by accident - buffer(0) gets bound)
- No scalar arguments

**Currently broken**:
- Multiple array arguments (buffer indices > 0)
- Any kernel with scalar parameters
- Mixed array + scalar arguments

### Proposed Fix Options

#### Option A: Packed Struct Approach (NVPTX-style)
Keep single buffer binding but change kernel signature generation:
```metal
struct kernel_args {
    device float* a;
    device float* b;
    float c;
};

[[kernel]] void kernel(
    constant kernel_args& args [[buffer(0)]],
    uint3 tid [[thread_position_in_grid]]
)
```

**Pros**: Minimal runtime changes, works with current GOMP argument packing
**Cons**: Non-idiomatic Metal code, loses separate buffer optimization

#### Option B: Individual Buffer Bindings (Metal-native)
Fix runtime to bind each argument separately:
```cpp
// In metal_bind_kernel_arguments:
void **args_array = (void **)dp;
int buffer_idx = 0;

for (each kernel argument) {
    if (is_pointer) {
        MTL::Buffer *buf = metal_find_buffer(ord, args_array[i]);
        enc->setBuffer(buf, 0, buffer_idx++);
    } else {
        // Handle scalar - need separate mechanism
    }
}
```

**Pros**: Idiomatic Metal, better performance, matches generated code
**Cons**: Requires significant runtime changes, needs argument metadata

#### Option C: Hybrid Approach
- Pointer arrays as separate buffers (0, 1, 2, ...)
- All scalars packed into final buffer (N)

**Pros**: Balances idiom with practicality
**Cons**: Complex, needs both approaches

### Recommended Solution

**Implement Option B** with argument descriptor metadata:

1. **mkoffload**: Generate argument type info for each kernel
2. **Runtime**: Iterate through arguments, binding each appropriately
3. **Scalars**: Use Metal's `setBytes()` for small scalars, buffer for larger

This matches Metal's design philosophy and enables future optimizations.

### Verification Test Cases

```fortran
! Test 1: Single array
!$acc parallel loop
do i = 1, n
  a(i) = i
end do

! Test 2: Two arrays
!$acc parallel loop
do i = 1, n
  a(i) = b(i)
end do

! Test 3: Scalar parameter
!$acc parallel loop
do i = 1, n
  a(i) = c
end do

! Test 4: Multiple arrays + scalar
!$acc parallel loop
do i = 1, n
  a(i) = b(i) + c(i) + d
end do
```

### Current Status

**Working**:
- ✅ Kernel signature generation (metal.cc)
- ✅ Single buffer binding infrastructure (plugin-metal.cpp)

**Broken**:
- ❌ Multiple buffer bindings
- ❌ Scalar argument passing
- ❓ Array descriptor unwrapping (needs verification)

### Next Steps

1. Create simple reproducer to confirm buffer binding issue
2. Add argument metadata to mkoffload output
3. Implement multi-buffer binding in plugin-metal.cpp
4. Add scalar argument support via setBytes()
5. Verify with full test suite

---

**Analysis Date**: 2025-12-26
**Branch**: accelerate-metal
**Issue**: #39
