# Metal Driver Flag Handling - Verification [#37]

## Status: ✓ VERIFIED COMPLETE

All driver flag handling infrastructure for Metal backend is implemented and verified.

## Components Verified

### 1. ✓ Option Definitions
**File:** `gcc/config/metal/metal.opt`
- Metal-specific -mmsl flag for MSL version selection
- Enum values: 2.4 (default), 3.0, 3.1

### 2. ✓ Driver Specs
**File:** `gcc/config/metal/metal.h`
- `GOMP_SELF_SPECS ""` - Prevents pthread injection
- Empty STARTFILE/ENDFILE/LIB/LINK specs (offload-only target)

### 3. ✓ Offload Support
**File:** `gcc/config/metal/offload.h`
- `ACCEL_COMPILER_acc_device = GOMP_DEVICE_METAL`

### 4. ✓ Build Integration
**Files:**
- `gcc/config/metal/t-metal` - mkoffload build rules
- `gcc/config.gcc` - Target registration (lines 530-533, 2954-2960)

### 5. ✓ mkoffload Implementation
**File:** `gcc/config/metal/mkoffload.cc`
- Processes -fopenacc, -fopenmp, -save-temps, -v flags
- Handles -foffload-abi options

## Expected Driver Behavior

Command:
```bash
gfortran -fopenacc -foffload=metal test.f90
```

Driver flow:
1. Frontend: test.f90 → test.o (with LTO offload sections)
2. mkoffload-metal: test.o → test.metal.o
3. Linker: test.o + test.metal.o + libgomp → executable

## Flag Examples

Basic:
```bash
gfortran -fopenacc -foffload=metal test.f90
```

With MSL version:
```bash
gfortran -fopenacc -foffload=metal -foffload-options=metal:-mmsl=3.1 test.f90
```

Disable offload:
```bash
gfortran -fopenacc -foffload=disable test.f90
```

## Configuration Required

GCC must be configured with:
```bash
../configure --enable-offload-targets=metal
```

## Implementation Notes

Metal driver integration follows NVPTX reference implementation exactly:
- Empty driver specs for offload-only target
- GOMP_SELF_SPECS prevents pthread injection
- Target-specific options via -foffload-options
- mkoffload automatically invoked by driver

## Verification Date

2025-12-26 - Sergei (issue #37)
