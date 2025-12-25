# Testing Metal mkoffload

## Overview

The Metal mkoffload tool integrates Metal shader compilation into the GCC offload workflow.

## Compilation Pipeline

1. `.metal` source → `.air` (Metal IR) via `xcrun metal`
2. `.air` → `.metallib` (Metal library) via `xcrun metallib`
3. `.metallib` → embedded C source → `.o` object file

## Manual Testing

```bash
# Create a simple Metal shader
cat > test.metal << 'EOF'
#include <metal_stdlib>
using namespace metal;

kernel void simple_add(device float *a [[buffer(0)]],
                       device float *b [[buffer(1)]],
                       device float *result [[buffer(2)]],
                       uint id [[thread_position_in_grid]])
{
    result[id] = a[id] + b[id];
}
EOF

# Set required environment variables
export COLLECT_GCC=/usr/bin/gcc

# Invoke mkoffload
./metal-mkoffload -fopenmp -v -save-temps test.metal -o test.o

# Check generated files
ls -l test.metal test.air test.metallib test.mkoffload.c test.o
```

## Compilation Options

- `-g` - Include debug information in Metal IR
- `-O0`, `-O1`, `-O2`, `-O3` - Optimization level
- `-Os`, `-Ofast` - Size/speed optimization (mapped to -O2)
- `-v` - Verbose output showing all commands
- `-save-temps` - Keep intermediate files

## Expected Behavior

1. Metal shader compiles to AIR without errors
2. AIR links to metallib successfully
3. Metallib embedded as byte array in C source
4. C source compiles to object file containing `__metal_library` symbol

## Requirements

- macOS with Xcode Command Line Tools installed
- `xcrun`, `metal`, and `metallib` available in PATH
- COLLECT_GCC environment variable set

## Known Limitations

- Input must be valid Metal Shading Language (.metal) source
- Metal compiler backend (GIMPLE → MSL) not yet implemented
- Runtime library integration pending
