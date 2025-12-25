#!/bin/bash

# Script to run Metal OpenACC integration tests
# Usage: ./scripts/run-metal-tests.sh [test-name]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${ROOT_DIR}/gcc-build"
TEST_DIR="${ROOT_DIR}/libgomp/testsuite/libgomp.oacc-fortran"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    echo "Please build GCC first:"
    echo "  mkdir -p gcc-build && cd gcc-build"
    echo "  ../gcc/configure --enable-languages=fortran \\"
    echo "                   --enable-offload-targets=metal \\"
    echo "                   --disable-multilib --disable-bootstrap"
    echo "  make -j\$(nproc)"
    exit 1
fi

# Check if gfortran exists
if [ ! -x "$BUILD_DIR/gcc/gfortran" ]; then
    echo "Error: gfortran not found in build directory"
    echo "Please complete the build first"
    exit 1
fi

GFORTRAN="$BUILD_DIR/gcc/gfortran"
GFORTRAN_FLAGS="-B $BUILD_DIR/gcc -fopenacc -foffload=metal"

# Metal test files
METAL_TESTS=(
    "metal-data-transfer.f90"
    "metal-parallel-loop.f90"
    "metal-kernels.f90"
    "metal-async.f90"
    "metal-reduction.f90"
    "metal-memory.f90"
    "metal-combined.f90"
)

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Run a single test
run_test() {
    local test_file="$1"
    local test_path="$TEST_DIR/$test_file"
    local test_name="${test_file%.f90}"
    local output_bin="/tmp/${test_name}_$$"

    if [ ! -f "$test_path" ]; then
        echo -e "${RED}FAIL${NC}: $test_name (file not found)"
        return 1
    fi

    echo -n "Testing $test_name ... "

    # Compile
    if ! "$GFORTRAN" $GFORTRAN_FLAGS "$test_path" -o "$output_bin" 2>/tmp/compile_$$.log; then
        echo -e "${RED}FAIL${NC} (compilation error)"
        cat /tmp/compile_$$.log
        rm -f /tmp/compile_$$.log
        return 1
    fi
    rm -f /tmp/compile_$$.log

    # Run
    if ! "$output_bin" > /tmp/run_$$.log 2>&1; then
        local exit_code=$?
        echo -e "${RED}FAIL${NC} (exit code: $exit_code)"
        cat /tmp/run_$$.log
        rm -f "$output_bin" /tmp/run_$$.log
        return 1
    fi

    # Check for unexpected output
    if [ -s /tmp/run_$$.log ]; then
        echo -e "${YELLOW}WARN${NC} (unexpected output)"
        cat /tmp/run_$$.log
        rm -f "$output_bin" /tmp/run_$$.log
        return 0
    fi

    echo -e "${GREEN}PASS${NC}"
    rm -f "$output_bin" /tmp/run_$$.log
    return 0
}

# Run with DejaGnu
run_dejagnu() {
    echo "Running Metal tests via DejaGnu..."
    cd "$BUILD_DIR/gcc"

    if [ -z "$1" ]; then
        make check-gfortran RUNTESTFLAGS="oacc-fortran.exp=metal-*.f90"
    else
        make check-gfortran RUNTESTFLAGS="oacc-fortran.exp=$1"
    fi

    echo
    echo "Summary:"
    grep -E "^(PASS|FAIL|UNSUPPORTED).*metal" \
        testsuite/gfortran/gfortran.sum || echo "No results found"
}

# Main execution
main() {
    echo "Metal OpenACC Integration Test Runner"
    echo "======================================"
    echo

    # Check for DejaGnu mode
    if [ "$1" = "--dejagnu" ] || [ "$1" = "-d" ]; then
        shift
        run_dejagnu "$@"
        exit $?
    fi

    # Check for help
    if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
        echo "Usage: $0 [OPTIONS] [test-name]"
        echo
        echo "Options:"
        echo "  -d, --dejagnu    Run via DejaGnu test framework"
        echo "  -h, --help       Show this help message"
        echo
        echo "Test names:"
        for test in "${METAL_TESTS[@]}"; do
            echo "  ${test%.f90}"
        done
        echo
        echo "Examples:"
        echo "  $0                           # Run all tests"
        echo "  $0 metal-data-transfer.f90   # Run specific test"
        echo "  $0 --dejagnu                 # Run via DejaGnu"
        exit 0
    fi

    # Run specific test if provided
    if [ -n "$1" ]; then
        run_test "$1"
        exit $?
    fi

    # Run all tests
    local passed=0
    local failed=0

    for test in "${METAL_TESTS[@]}"; do
        if run_test "$test"; then
            ((passed++))
        else
            ((failed++))
        fi
    done

    echo
    echo "======================================"
    echo "Results: ${GREEN}${passed} passed${NC}, ${RED}${failed} failed${NC}"
    echo

    [ $failed -eq 0 ]
}

main "$@"
