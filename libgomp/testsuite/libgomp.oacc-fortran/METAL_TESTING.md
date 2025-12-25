# Metal OpenACC Integration Testing

## Overview

This directory contains comprehensive integration tests for the Metal GPU backend
in GFortran's OpenACC implementation. These tests validate the complete pipeline
from Fortran source code through the Metal runtime to GPU execution.

## Test Suite Structure

### Core Functionality Tests

1. **metal-data-transfer.f90**
   - Basic data transfer operations (enter data, exit data)
   - Tests copyin, copyout, create, delete directives
   - Validates acc_is_present for device memory management
   - **Coverage**: Data movement, reference counting basics

2. **metal-parallel-loop.f90**
   - Parallel loop execution with various patterns
   - Gang, worker, vector parallelism
   - Explicit data clauses vs implicit copy
   - In-place array modifications
   - **Coverage**: Parallel execution, work distribution

3. **metal-kernels.f90**
   - Kernels construct with automatic parallelization
   - Single and nested loop parallelization
   - Multiple independent loops in kernels region
   - **Coverage**: Automatic parallelization, compiler analysis

4. **metal-async.f90**
   - Asynchronous operations with queue management
   - Multiple concurrent async queues
   - Async dependency chains
   - Wait clause synchronization
   - Async with data regions
   - **Coverage**: Concurrency, GPU command queues, synchronization

5. **metal-reduction.f90**
   - Reduction operations (+, *, max, min)
   - Integer and floating-point reductions
   - Multiple reductions in same construct
   - Gang/worker/vector reductions
   - Kernels construct reductions
   - **Coverage**: Parallel reductions, correctness validation

6. **metal-memory.f90**
   - Memory management and reference counting
   - Allocatable array handling
   - Nested data regions
   - Copy vs copyin vs copyout semantics
   - Present clause validation
   - **Coverage**: Memory lifecycle, reference counting

7. **metal-combined.f90**
   - Combined features: async + reduction + data transfer
   - Complex async dependency graphs
   - Multiple operations on shared data
   - **Coverage**: Integration, real-world patterns

## Running the Tests

### Prerequisites

1. **Metal-enabled GFortran build**:
   ```bash
   # In gcc-build directory
   ../gcc/configure --enable-languages=fortran \
                    --enable-offload-targets=metal \
                    --disable-multilib \
                    --disable-bootstrap
   make -j32
   ```

2. **macOS system** with Metal support (macOS 10.13+)

3. **Compatible GPU**: Any Apple GPU (integrated or discrete)

### Full Test Suite

Run all Metal tests from gcc-build/gcc:

```bash
cd gcc-build/gcc
make check-gfortran RUNTESTFLAGS="oacc-fortran.exp=metal-*.f90"
```

### Individual Tests

Run a specific test:

```bash
cd gcc-build/gcc
make check-gfortran RUNTESTFLAGS="oacc-fortran.exp=metal-data-transfer.f90"
```

### Manual Execution

For debugging, run tests manually:

```bash
# Build
gcc-build/gcc/gfortran -B gcc-build/gcc \
    -fopenacc -foffload=metal \
    libgomp/testsuite/libgomp.oacc-fortran/metal-data-transfer.f90 \
    -o /tmp/test_metal

# Run
/tmp/test_metal
echo "Exit code: $?"
```

## Test Output

### Success

Tests exit with code 0 on success. No output indicates all validations passed.

### Failure

Tests use unique stop codes for each validation:
- stop 1, stop 2, etc. indicate specific failure points
- Check test source for corresponding validation

Example failure:
```
Test 2 (async queues) failed:           5 errors
STOP 2
```

### DejaGnu Integration

Tests marked with:
- `{ dg-do run }` - Compile and execute
- `{ dg-skip-if "Metal not available" { ! openacc_metal_selected } }` - Skip if no Metal

## Expected Results

All tests should show:
```
PASS: libgomp.oacc-fortran/metal-data-transfer.f90 -DACC_DEVICE_TYPE_metal=1
PASS: libgomp.oacc-fortran/metal-parallel-loop.f90 -DACC_DEVICE_TYPE_metal=1
...
```

If Metal is not available:
```
UNSUPPORTED: libgomp.oacc-fortran/metal-data-transfer.f90
```

## Debugging Failed Tests

### Enable Runtime Diagnostics

Set environment variables for verbose output:

```bash
export GOMP_DEBUG=1                    # libgomp debug output
export ACC_DEVICE_NUM=0                # Explicit device selection
```

### Check GPU Availability

Verify Metal device detection:

```fortran
use openacc
integer :: num_devices

num_devices = acc_get_num_devices(acc_device_metal)
print *, "Metal devices available:", num_devices
```

### Inspect Generated Code

View Metal kernel code:

```bash
gfortran -fopenacc -foffload=metal -fdump-tree-all test.f90
```

Examine:
- `*.omplower` - OpenACC lowering
- `*.metal` - Generated Metal shader code (if available)

### Memory Debugging

For memory leaks or corruption:

```bash
# Run with memory sanitizer (if available)
ASAN_OPTIONS=detect_leaks=1 ./test

# Or use Xcode Instruments for GPU memory profiling
instruments -t "Metal System Trace" ./test
```

## Validation Strategy

Each test validates results using:

1. **Exact comparison** for integer operations
2. **Epsilon comparison** for floating-point (typically 1.0e-12)
3. **Reference CPU computation** for expected results
4. **Error counting** to report failures comprehensively

Example validation pattern:
```fortran
errors = 0
do i = 1, n
    if (abs(result(i) - expected(i)) > 1.0e-12_dp) then
        errors = errors + 1
    end if
end do

if (errors > 0) then
    print *, "Test failed:", errors, "errors"
    stop 1
end if
```

## Known Limitations

1. **ACC_MEM_SHARED**: Metal uses discrete GPU memory (ACC_MEM_SHARED=0).
   Tests account for explicit data transfer requirements.

2. **Reduction precision**: Large reductions may accumulate floating-point
   error. Tests use appropriate epsilon values.

3. **Async ordering**: Metal command buffers enforce ordering within queues
   but allow parallelism across queues. Tests validate this behavior.

## Adding New Tests

When creating new Metal tests:

1. **Use consistent naming**: `metal-<feature>.f90`

2. **Add DejaGnu directives**:
   ```fortran
   ! { dg-do run }
   ! { dg-skip-if "Metal not available" { ! openacc_metal_selected } }
   ! { dg-additional-options "-fopenacc" }
   ```

3. **Include validation**:
   - Count errors, don't stop on first failure
   - Use unique stop codes
   - Print diagnostic messages

4. **Test both positive and edge cases**:
   - Normal operation
   - Boundary conditions
   - Error handling

5. **Document expected behavior** in comments

## Integration with CI

These tests are designed for integration into GCC's test suite:

```bash
# From gcc-build/gcc
make -j32 -k check-gfortran > /tmp/test.log 2>&1

# Check results
grep -E "^FAIL|^PASS.*metal" gcc/testsuite/gfortran/gfortran.sum
```

## References

- GCC OpenACC: https://gcc.gnu.org/wiki/OpenACC
- Metal Shading Language: https://developer.apple.com/metal/
- libgomp documentation: gcc/libgomp/libgomp.texi
- Test framework: gcc/testsuite/lib/libgomp-dg.exp
