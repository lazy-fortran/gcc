# Metal Offload Image Format

## Overview

The Metal offload image packages compiled `.metallib` binaries with symbol tables
for runtime loading by libgomp's Metal plugin. This format follows the pattern
established by NVPTX but adapted for Metal's binary library format.

## Image Structure

The mkoffload tool generates a C source file that embeds:

1. **Binary metallib data** - The compiled Metal library as a byte array
2. **Function symbol table** - Maps host function names to Metal kernel names
3. **Variable symbol table** - Maps host variable names to Metal buffer names
4. **Target data structure** - Container for all image metadata
5. **Registration code** - Constructor/destructor for GOMP registration

## Generated C Code Structure

```c
#include <stdint.h>

/* Embedded metallib binary */
static const unsigned char metallib_data[] = {
  0x4d, 0x54, 0x4c, 0x42, /* MTLB magic */
  /* ... full metallib bytes ... */
};

/* Metallib container */
static const struct metallib_obj {
  const unsigned char *data;
  __SIZE_TYPE__ size;
} metallib_obj = {
  metallib_data, sizeof (metallib_data)
};

/* Symbol tables */
static const char *const var_mappings[] = {
  "var1", "var2" /* ... */
};

static const char *const func_mappings[] = {
  "kernel1", "kernel2" /* ... */
};

/* Metal-specific target data */
static const struct metal_data {
  uintptr_t omp_requires_mask;
  const struct metallib_obj *metallib;
  const char *const *var_names;
  unsigned var_num;
  const char *const *fn_names;
  unsigned fn_num;
} metal_data = {
  0x00000000,          /* OMP requires mask */
  &metallib_obj,
  var_mappings,
  sizeof (var_mappings) / sizeof (var_mappings[0]),
  func_mappings,
  sizeof (func_mappings) / sizeof (func_mappings[0])
};

/* Registration functions */
extern void GOMP_offload_register_ver (unsigned, const void *, int, const void *);
extern void GOMP_offload_unregister_ver (unsigned, const void *, int, const void *);
extern const void *const __OFFLOAD_TABLE__[];

static __attribute__((constructor)) void init (void)
{
  GOMP_offload_register_ver (0x01010009, __OFFLOAD_TABLE__, 9, &metal_data);
}

static __attribute__((destructor)) void fini (void)
{
  GOMP_offload_unregister_ver (0x01010009, __OFFLOAD_TABLE__, 9, &metal_data);
}
```

## Version Constants

- `GOMP_DEVICE_METAL` = 9
- `GOMP_VERSION_METAL` = 1
- Version packed as: `GOMP_VERSION_PACK(GOMP_VERSION, GOMP_VERSION_METAL)`

## Runtime Loading Process

1. **Registration**: Constructor calls `GOMP_offload_register_ver` with `metal_data`
2. **Image loading**: Plugin's `GOMP_OFFLOAD_load_image` receives `metal_data`
3. **Library creation**: `MTL::Device::newLibrary(dispatch_data)` loads metallib
4. **Kernel compilation**: Create `MTL::ComputePipelineState` for each function
5. **Symbol resolution**: Return kernel pipeline addresses in target_table

## Key Differences from NVPTX

| Aspect | NVPTX | Metal |
|--------|-------|-------|
| Binary format | PTX text or cubin | .metallib binary |
| Loading | CUDA driver API | Metal framework |
| Kernel handle | CUfunction | MTL::ComputePipelineState |
| JIT compilation | Optional (PTX) | Done during library creation |
| Symbol embedding | Text annotations | Binary library introspection |

## Symbol Table Population

Currently symbol tables are empty placeholders. Future enhancements will:

1. Parse metallib binary to extract kernel names
2. Match kernel names to host function symbols
3. Populate func_mappings with correct kernel identifiers
4. Support variable mappings for global/constant buffers

## File Flow

```
Input: compiled.metallib (binary Metal library)
       ├─> read_file() reads entire binary
       └─> process() embeds as byte array

Output: compiled.c (C source with embedded data)
        ├─> compile_native() compiles to .o
        └─> linked into host executable
```

## Compatibility Notes

- Requires macOS 10.13+ (Metal 2)
- Apple Silicon preferred (unified memory)
- Intel Macs supported but may have different memory models
- metallib format is architecture-specific (arm64 vs x86_64)

## Future Work

- Metallib parsing to extract kernel metadata
- Variable mapping support for global buffers
- Multi-architecture fat metallib support
- Debug symbol preservation
- Pipeline optimization flags
