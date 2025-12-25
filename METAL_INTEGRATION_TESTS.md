# Metal OpenACC Integration Tests

## Purpose

End-to-end integration tests for the Metal GPU backend in GFortran's OpenACC
implementation. These tests validate the complete pipeline from Fortran source
code through compiler transformations, runtime library, and Metal execution on
Apple GPUs.

## Quick Start

### Running Tests via DejaGnu

```bash
cd gcc-build/gcc
make check-gfortran RUNTESTFLAGS="oacc-fortran.exp=metal-*.f90"
```

### Running Tests via Script

```bash
./scripts/run-metal-tests.sh
```

### Running Individual Test

```bash
./scripts/run-metal-tests.sh metal-data-transfer.f90
```

## Test Coverage

| Test | Coverage |
|------|----------|
| **metal-data-transfer.f90** | Data movement (enter/exit data, copyin/copyout/create) |
| **metal-parallel-loop.f90** | Parallel execution, work distribution, gang/worker/vector |
| **metal-kernels.f90** | Automatic parallelization, compiler analysis |
| **metal-async.f90** | Async queues, concurrency, synchronization |
| **metal-reduction.f90** | Parallel reductions (+, *, max, min) |
| **metal-memory.f90** | Memory lifecycle, reference counting, allocatable arrays |
| **metal-combined.f90** | Integration scenarios, real-world patterns |

## Test Architecture

### Test Design Principles

1. **Self-validating**: Each test computes expected results and validates
2. **Comprehensive error reporting**: Count all errors, report totals
3. **Unique exit codes**: Each validation has unique stop code
4. **GPU vs CPU validation**: GPU results compared to CPU reference
5. **Epsilon-based comparison**: Appropriate tolerance for floating-point

### Test Structure Template

```fortran
! { dg-do run }
! { dg-skip-if "Metal not available" { ! openacc_metal_selected } }
! { dg-additional-options "-fopenacc" }

program metal_test
    use openacc
    use iso_fortran_env, only: dp => real64
    implicit none

    ! Initialize test data
    ! ...

    ! Execute on GPU
    !$acc parallel loop
    ! ...
    !$acc end parallel loop

    ! Validate results
    errors = 0
    do i = 1, n
        if (abs(gpu_result(i) - expected(i)) > epsilon) then
            errors = errors + 1
        end if
    end do

    if (errors > 0) then
        print *, "Test failed:", errors, "errors"
        stop 1
    end if
end program
```

## Prerequisites

### Build Configuration

Metal backend must be enabled during GCC build:

```bash
mkdir -p gcc-build && cd gcc-build
../gcc/configure \
    --enable-languages=fortran \
    --enable-offload-targets=metal \
    --disable-multilib \
    --disable-bootstrap \
    CFLAGS='-Og -g' \
    CXXFLAGS='-Og -g'
make -j$(sysctl -n hw.ncpu)
```

### System Requirements

- macOS 10.13 or later
- Apple GPU (integrated or discrete)
- Xcode Command Line Tools
- Metal framework

### Verify Metal Availability

```bash
# Check Metal device detection
system_profiler SPDisplaysDataType | grep "Metal"

# Or programmatically:
cat > /tmp/check_metal.f90 <<'EOF'
program check_metal
    use openacc
    integer :: n
    n = acc_get_num_devices(acc_device_metal)
    print *, "Metal devices:", n
end program
EOF

gcc-build/gcc/gfortran -B gcc-build/gcc -fopenacc /tmp/check_metal.f90 -o /tmp/check
/tmp/check
```

## Running Tests

### Method 1: DejaGnu Test Framework (Recommended)

```bash
cd gcc-build/gcc

# All Metal tests
make check-gfortran RUNTESTFLAGS="oacc-fortran.exp=metal-*.f90"

# Specific test
make check-gfortran RUNTESTFLAGS="oacc-fortran.exp=metal-reduction.f90"

# View results
grep "metal" testsuite/gfortran/gfortran.sum
```

### Method 2: Test Runner Script

```bash
# All tests with colored output
./scripts/run-metal-tests.sh

# Specific test
./scripts/run-metal-tests.sh metal-async.f90

# Via DejaGnu (from script)
./scripts/run-metal-tests.sh --dejagnu

# Help
./scripts/run-metal-tests.sh --help
```

### Method 3: Manual Compilation and Execution

```bash
# Compile
gcc-build/gcc/gfortran -B gcc-build/gcc \
    -fopenacc -foffload=metal \
    libgomp/testsuite/libgomp.oacc-fortran/metal-data-transfer.f90 \
    -o /tmp/test

# Run
/tmp/test
echo "Exit code: $?"
```

## Interpreting Results

### Success

- Exit code: 0
- No output (all validations passed)
- DejaGnu: `PASS: libgomp.oacc-fortran/metal-*.f90`

### Failure

- Exit code: 1-99 (unique stop code identifies failure location)
- Error message printed to stdout/stderr
- DejaGnu: `FAIL: libgomp.oacc-fortran/metal-*.f90`

Example:
```
Test 3 (dependency chain) failed:           12 errors
STOP 3
```

Check test source at `stop 3` to identify failing validation.

### Unsupported

- Metal backend not available or disabled
- DejaGnu: `UNSUPPORTED: libgomp.oacc-fortran/metal-*.f90`

## Debugging Failed Tests

### Enable Verbose Output

```bash
export GOMP_DEBUG=1        # libgomp diagnostics
export ACC_DEVICE_NUM=0    # Force specific device
```

### Examine Compilation

```bash
gfortran -B gcc-build/gcc -fopenacc -foffload=metal \
    -fdump-tree-all \
    -fdump-rtl-all \
    test.f90

# Check intermediate representations
ls -la *.{original,gimple,omplower}
```

### Runtime Debugging

```bash
# Memory issues
ASAN_OPTIONS=detect_leaks=1 ./test

# GPU profiling (macOS)
instruments -t "Metal System Trace" ./test

# Or Xcode GPU debugger
# Product > Scheme > Edit Scheme > Run > Options > Metal API Validation
```

### Common Issues

1. **Metal device not found**:
   - Verify system has compatible GPU
   - Check Metal framework is available
   - Ensure offload target configured correctly

2. **Compilation errors**:
   - Verify build completed successfully
   - Check Metal backend is enabled (`gfortran -v`)
   - Ensure libgomp built with Metal support

3. **Runtime crashes**:
   - Enable debug output (`GOMP_DEBUG=1`)
   - Check for memory corruption
   - Verify data transfer semantics

4. **Incorrect results**:
   - Compare with CPU execution
   - Check floating-point epsilon tolerances
   - Verify async synchronization

## Test Maintenance

### Adding New Tests

1. Create test file: `libgomp/testsuite/libgomp.oacc-fortran/metal-<feature>.f90`

2. Include required directives:
   ```fortran
   ! { dg-do run }
   ! { dg-skip-if "Metal not available" { ! openacc_metal_selected } }
   ! { dg-additional-options "-fopenacc" }
   ```

3. Add to test runner: Edit `scripts/run-metal-tests.sh`, add to `METAL_TESTS` array

4. Document in `METAL_TESTING.md`

### Updating Existing Tests

- Maintain backward compatibility
- Update epsilon tolerances if needed
- Document changes in test comments
- Verify all stop codes remain unique

## Integration with CI

These tests integrate with GCC's existing test infrastructure:

```bash
# Full test suite
cd gcc-build/gcc
make -j32 -k check-gfortran > /tmp/test.log 2>&1

# Extract Metal results
grep -E "^(PASS|FAIL|UNSUPPORTED).*metal" \
    testsuite/gfortran/gfortran.sum
```

## Performance Benchmarking

While these are correctness tests, timing data can be collected:

```bash
# Run with timing
time ./test

# Or instrument test code:
!$acc parallel loop
call cpu_time(start)
! ... computation ...
call cpu_time(finish)
print *, "GPU time:", finish - start
!$acc end parallel loop
```

For detailed performance analysis, use dedicated benchmarks (separate from these
correctness tests).

## References

- **Test framework**: `libgomp/testsuite/libgomp.oacc-fortran/METAL_TESTING.md`
- **DejaGnu**: `gcc/testsuite/lib/libgomp-dg.exp`
- **Metal plugin**: `libgomp/plugin/plugin-metal.cpp`
- **OpenACC runtime**: `libgomp/openacc.f90`
- **GCC testing guide**: https://gcc.gnu.org/testing/

## License

Copyright (C) 2025 Free Software Foundation, Inc.

These tests are part of the GCC test suite and are licensed under the same
terms as GCC.
